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

#include "msgCore.h"
#include "nsDBFolderInfo.h"
#include "nsMsgDatabase.h"
#include "nsMsgFolderFlags.h"
#include "nsIPref.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIMsgDBView.h"

static const char *kDBFolderInfoScope = "ns:msg:db:row:scope:dbfolderinfo:all";
static const char *kDBFolderInfoTableKind = "ns:msg:db:table:kind:dbfolderinfo";

struct mdbOid gDBFolderInfoOID;

static const char *	kNumVisibleMessagesColumnName = "numVisMsgs";
static const char *	kNumMessagesColumnName ="numMsgs";
static const char *	kNumNewMessagesColumnName = "numNewMsgs";
static const char *	kFlagsColumnName = "flags";
static const char *	kLastMessageLoadedColumnName = "lastMsgLoaded";
static const char *	kFolderSizeColumnName = "folderSize";
static const char *	kExpungedBytesColumnName = "expungedBytes";
static const char *	kFolderDateColumnName = "folderDate";
static const char *	kHighWaterMessageKeyColumnName = "highWaterKey";

static const char *	kImapUidValidityColumnName = "UIDValidity";
static const char *	kTotalPendingMessagesColumnName = "totPendingMsgs";
static const char *	kUnreadPendingMessagesColumnName = "unreadPendingMsgs";
static const char * kMailboxNameColumnName = "mailboxName";
static const char * kKnownArtsSetColumnName = "knownArts";
static const char * kExpiredMarkColumnName = "expiredMark";
static const char * kVersionColumnName = "version";
static const char * kCharacterSetColumnName = "charSet";
static const char * kCharacterSetOverrideColumnName = "charSetOverride";
static const char * kLocaleColumnName = "locale";


#define kMAILNEWS_VIEW_DEFAULT_CHARSET        "mailnews.view_default_charset"
#define kMAILNEWS_DEFAULT_CHARSET_OVERRIDE    "mailnews.force_charset_override"
static nsString   gDefaultCharacterSet;
static PRBool     gDefaultCharacterOverride;
static nsIObserver *gFolderCharsetObserver = nsnull;
static PRBool     gInitializeObserver = PR_FALSE;
static PRBool     gReleaseObserver = PR_FALSE;

// observer for charset related preference notification
class nsFolderCharsetObserver : public nsIObserver {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsFolderCharsetObserver() { NS_INIT_ISUPPORTS(); }
  virtual ~nsFolderCharsetObserver() {}
};

NS_IMPL_ISUPPORTS1(nsFolderCharsetObserver, nsIObserver);

NS_IMETHODIMP nsFolderCharsetObserver::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsresult rv;

  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDependentString aTopicString(aTopic);
  if (aTopicString.Equals(NS_LITERAL_STRING("nsPref:changed")))
  {
    nsDependentString prefName(someData);
    
    if (prefName.Equals(NS_LITERAL_STRING(kMAILNEWS_VIEW_DEFAULT_CHARSET)))
    {
      PRUnichar *prefCharset = nsnull;
      rv = prefs->GetLocalizedUnicharPref(kMAILNEWS_VIEW_DEFAULT_CHARSET, &prefCharset);
      if (NS_SUCCEEDED(rv))
      {
        gDefaultCharacterSet.Assign(prefCharset);
        PR_Free(prefCharset);
      }
    }
    else if (prefName.Equals(NS_LITERAL_STRING(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE)))
    {
      rv = prefs->GetBoolPref(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, &gDefaultCharacterOverride);
    }
  }
  else if (aTopicString.Equals(NS_LITERAL_STRING(NS_XPCOM_SHUTDOWN_OBSERVER_ID)))
  {
    rv = prefs->RemoveObserver(kMAILNEWS_VIEW_DEFAULT_CHARSET, this);
    rv = prefs->RemoveObserver(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, this);
    gReleaseObserver = PR_TRUE;   // set true to release observer
  }

  return rv;
}


NS_IMPL_ADDREF(nsDBFolderInfo)
NS_IMPL_RELEASE(nsDBFolderInfo)

NS_IMETHODIMP
nsDBFolderInfo::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
    if(iid.Equals(NS_GET_IID(nsIDBFolderInfo)) ||
       iid.Equals(NS_GET_IID(nsISupports))) {
		*result = NS_STATIC_CAST(nsIDBFolderInfo*, this);
		AddRef();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}


nsDBFolderInfo::nsDBFolderInfo(nsMsgDatabase *mdb)
    :     m_flags(0),
          m_lastMessageLoaded(0),
          m_expiredMark(0),
          m_numVisibleMessagesColumnToken(0),
          m_expiredMarkColumnToken(0)
{
    NS_INIT_REFCNT();
	m_mdbTable = NULL;
	m_mdbRow = NULL;
	m_version = 1;			// for upgrading...
	m_csid = 0;				// default csid for these messages
	m_IMAPHierarchySeparator = 0;	// imap path separator
	// mail only (for now)
	m_folderSize = 0;
	m_folderDate = 0;
	m_expungedBytes = 0;	// sum of size of deleted messages in folder
	m_highWaterMessageKey = 0;

	m_numNewMessages = 0;
	m_numMessages = 0;
	m_numVisibleMessages = 0;
	// IMAP only
	m_ImapUidValidity = 0;
	m_totalPendingMessages =0;
	m_unreadPendingMessages = 0;

	m_mdbTokensInitialized = PR_FALSE;

  if (!gInitializeObserver)
  {
    gInitializeObserver = PR_TRUE;
    nsresult rv;
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      PRUnichar *prefCharset = nsnull;
      rv = prefs->GetLocalizedUnicharPref(kMAILNEWS_VIEW_DEFAULT_CHARSET, &prefCharset);
      if (NS_SUCCEEDED(rv))
      {
        gDefaultCharacterSet.Assign(prefCharset);
        PR_Free(prefCharset);
      }
      rv = prefs->GetBoolPref(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, &gDefaultCharacterOverride);

      gFolderCharsetObserver = new nsFolderCharsetObserver();
      NS_ASSERTION(gFolderCharsetObserver, "failed to create observer");

      // register prefs callbacks
      if (gFolderCharsetObserver)
      {
        NS_ADDREF(gFolderCharsetObserver);
        rv = prefs->AddObserver(kMAILNEWS_VIEW_DEFAULT_CHARSET, gFolderCharsetObserver);
        rv = prefs->AddObserver(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, gFolderCharsetObserver);

        // also register for shutdown
        nsCOMPtr<nsIObserverService> observerService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
          rv = observerService->AddObserver(gFolderCharsetObserver, NS_LITERAL_STRING(NS_XPCOM_SHUTDOWN_OBSERVER_ID).get());
        }
      }
    }
  }

	m_mdb = mdb;
	if (mdb)
	{
		mdb_err err;

//		mdb->AddRef();
		err = m_mdb->GetStore()->StringToToken(mdb->GetEnv(), kDBFolderInfoScope, &m_rowScopeToken); 
		if (err == NS_OK)
		{
			err = m_mdb->GetStore()->StringToToken(mdb->GetEnv(), kDBFolderInfoTableKind, &m_tableKindToken); 
			if (err == NS_OK)
			{
				gDBFolderInfoOID.mOid_Scope = m_rowScopeToken;
				gDBFolderInfoOID.mOid_Id = 1;
			}
		}
		InitMDBInfo();
	}
}

nsDBFolderInfo::~nsDBFolderInfo()
{
  if (gReleaseObserver) 
  {
    NS_IF_RELEASE(gFolderCharsetObserver);
  }

	if (m_mdb)
	{
		if (m_mdbTable)
			m_mdbTable->CutStrongRef(m_mdb->GetEnv());
		if (m_mdbRow)
			m_mdbRow->CutStrongRef(m_mdb->GetEnv());
		// nsMsgDatabase strictly owns nsDBFolderInfo, so don't ref-count db.
//		m_mdb->Release();
	}
}


// this routine sets up a new db to know about the dbFolderInfo stuff...
nsresult nsDBFolderInfo::AddToNewMDB()
{
	nsresult ret = NS_OK;
	if (m_mdb && m_mdb->GetStore())
	{
		nsIMdbStore *store = m_mdb->GetStore();
		// create the unique table for the dbFolderInfo.
		mdb_err err = store->NewTable(m_mdb->GetEnv(), m_rowScopeToken, 
			m_tableKindToken, PR_TRUE, nsnull, &m_mdbTable);

		// make sure the oid of the table is 1.
		struct mdbOid folderInfoTableOID;
		folderInfoTableOID.mOid_Id = 1;
		folderInfoTableOID.mOid_Scope = m_rowScopeToken;

//		m_mdbTable->BecomeContent(m_mdb->GetEnv(), &folderInfoTableOID);

		// create the singleton row for the dbFolderInfo.
		err  = store->NewRowWithOid(m_mdb->GetEnv(),
			&gDBFolderInfoOID, &m_mdbRow);

		// add the row to the singleton table.
		if (m_mdbRow && NS_SUCCEEDED(err))
		{
			err = m_mdbTable->AddRow(m_mdb->GetEnv(), m_mdbRow);
		}

		ret = err;	// what are we going to do about mdb_err's?
	}
	return ret;
}

nsresult nsDBFolderInfo::InitFromExistingDB()
{
	nsresult ret = NS_OK;
	if (m_mdb && m_mdb->GetStore())
	{
		nsIMdbStore *store = m_mdb->GetStore();
		if (store)
		{
			mdb_pos		rowPos;
			mdb_count outTableCount; // current number of such tables
			mdb_bool mustBeUnique; // whether port can hold only one of these
			mdb_bool hasOid;
			ret = store->GetTableKind(m_mdb->GetEnv(), m_rowScopeToken, m_tableKindToken, &outTableCount, 
				&mustBeUnique, &m_mdbTable);
//			NS_ASSERTION(mustBeUnique && outTableCount == 1, "only one global db info allowed");

			if (m_mdbTable)
			{
				// find singleton row for global info.
				ret = m_mdbTable->HasOid(m_mdb->GetEnv(), &gDBFolderInfoOID, &hasOid);
				if (ret == NS_OK)
				{
					nsIMdbTableRowCursor *rowCursor;
					rowPos = -1;
					ret= m_mdbTable->GetTableRowCursor(m_mdb->GetEnv(), rowPos, &rowCursor);
					if (ret == NS_OK)
					{
						ret = rowCursor->NextRow(m_mdb->GetEnv(), &m_mdbRow, &rowPos);
						rowCursor->CutStrongRef(m_mdb->GetEnv());
						if (ret == NS_OK && m_mdbRow)
						{
							LoadMemberVariables();
						}
					}
				}
			}
		}
	}
	return ret;
}

nsresult nsDBFolderInfo::InitMDBInfo()
{
	nsresult ret = NS_OK;
	if (!m_mdbTokensInitialized && m_mdb && m_mdb->GetStore())
	{
		nsIMdbStore *store = m_mdb->GetStore();
		nsIMdbEnv	*env = m_mdb->GetEnv();

		store->StringToToken(env,  kNumVisibleMessagesColumnName, &m_numVisibleMessagesColumnToken);
		store->StringToToken(env,  kNumMessagesColumnName, &m_numMessagesColumnToken);
		store->StringToToken(env,  kNumNewMessagesColumnName, &m_numNewMessagesColumnToken);
		store->StringToToken(env,  kFlagsColumnName, &m_flagsColumnToken);
		store->StringToToken(env,  kLastMessageLoadedColumnName, &m_lastMessageLoadedColumnToken);
		store->StringToToken(env,  kFolderSizeColumnName, &m_folderSizeColumnToken);
		store->StringToToken(env,  kExpungedBytesColumnName, &m_expungedBytesColumnToken);
		store->StringToToken(env,  kFolderDateColumnName, &m_folderDateColumnToken);

		store->StringToToken(env,  kHighWaterMessageKeyColumnName, &m_highWaterMessageKeyColumnToken);
		store->StringToToken(env,  kMailboxNameColumnName, &m_mailboxNameColumnToken);

		store->StringToToken(env,  kImapUidValidityColumnName, &m_imapUidValidityColumnToken);
		store->StringToToken(env,  kTotalPendingMessagesColumnName, &m_totalPendingMessagesColumnToken);
		store->StringToToken(env,  kUnreadPendingMessagesColumnName, &m_unreadPendingMessagesColumnToken);
		store->StringToToken(env,  kExpiredMarkColumnName, &m_expiredMarkColumnToken);
		store->StringToToken(env,  kVersionColumnName, &m_versionColumnToken);
		m_mdbTokensInitialized  = PR_TRUE;
	}
	return ret;
}

nsresult nsDBFolderInfo::LoadMemberVariables()
{
	nsresult ret = NS_OK;
	// it's really not an error for these properties to not exist...
	GetInt32PropertyWithToken(m_numVisibleMessagesColumnToken, m_numVisibleMessages);
	GetInt32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
	GetInt32PropertyWithToken(m_numNewMessagesColumnToken, m_numNewMessages);
	GetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
	GetInt32PropertyWithToken(m_folderSizeColumnToken, m_folderSize);
	GetInt32PropertyWithToken(m_folderDateColumnToken, (PRInt32 &) m_folderDate);
	GetInt32PropertyWithToken(m_imapUidValidityColumnToken, m_ImapUidValidity);
	GetInt32PropertyWithToken(m_expiredMarkColumnToken, (PRInt32 &) m_expiredMark);
	GetInt32PropertyWithToken(m_expungedBytesColumnToken, (PRInt32 &) m_expungedBytes);
  GetInt32PropertyWithToken(m_highWaterMessageKeyColumnToken, (PRInt32 &) m_highWaterMessageKey);
	PRInt32 version;

	GetInt32PropertyWithToken(m_versionColumnToken, version);
	m_version = (PRUint16) version;
	return ret;
}

NS_IMETHODIMP nsDBFolderInfo::SetVersion(PRUint32 version)
{
	m_version = version; 
	return SetUint32PropertyWithToken(m_versionColumnToken, (PRUint32) m_version);
}

NS_IMETHODIMP nsDBFolderInfo::GetVersion(PRUint32 *version)
{
	*version = m_version; 
	return NS_OK;
}


NS_IMETHODIMP nsDBFolderInfo::SetHighWater(nsMsgKey highWater, PRBool force)
{
	if (force || m_highWaterMessageKey < highWater)
		m_highWaterMessageKey = highWater;

	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetHighWater(nsMsgKey highWater)
{
	return SetHighWater(highWater, PR_TRUE);
}


NS_IMETHODIMP
nsDBFolderInfo::GetFolderSize(PRUint32 *size)
{
  if (!size) 
	  return NS_ERROR_NULL_POINTER;
  *size = m_folderSize;
  return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetFolderSize(PRUint32 size)
{
	m_folderSize = size;
	return SetUint32PropertyWithToken(m_folderSizeColumnToken, m_folderSize);
}

NS_IMETHODIMP
nsDBFolderInfo::GetFolderDate(PRUint32 *folderDate)
{
  if (!folderDate) 
	  return NS_ERROR_NULL_POINTER;
  *folderDate = m_folderDate;
  return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetFolderDate(PRUint32 folderDate)
{
	m_folderDate = folderDate;
	return SetUint32PropertyWithToken(m_folderDateColumnToken, folderDate);
}


NS_IMETHODIMP	nsDBFolderInfo::GetHighWater(nsMsgKey *result) 
{
	*result = m_highWaterMessageKey;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetExpiredMark(nsMsgKey expiredKey)
{
	m_expiredMark = expiredKey;
	return SetUint32PropertyWithToken(m_expiredMarkColumnToken, expiredKey);
}

NS_IMETHODIMP	nsDBFolderInfo::GetExpiredMark(nsMsgKey *result) 
{
	*result = m_expiredMark;
	return NS_OK;
}

NS_IMETHODIMP
nsDBFolderInfo::ChangeExpungedBytes(PRInt32 delta)
{
    return SetExpungedBytes(m_expungedBytes + delta);
}

PRBool nsDBFolderInfo::AddLaterKey(nsMsgKey key, PRTime until)
{
	//ducarroz: if until represente a folder time stamp,
	//          therefore it should be declared as a PRInt32.
	//          Else, it should be a PRTime.
	return PR_FALSE;
}

PRInt32	nsDBFolderInfo::GetNumLatered()
{
	return 0;
}

nsMsgKey	nsDBFolderInfo::GetLateredAt(PRInt32 laterIndex, PRTime pUntil)
{
	//ducarroz: if until represente a folder time stamp,
	//          therefore it should be declared as a PRInt32.
	//          Else, it should be a PRTime.
	return nsMsgKey_None;
}

void nsDBFolderInfo::RemoveLateredAt(PRInt32 laterIndex)
{
}

NS_IMETHODIMP nsDBFolderInfo::SetMailboxName(nsString *newBoxName)
{
	return SetPropertyWithToken(m_mailboxNameColumnToken, newBoxName);
}

NS_IMETHODIMP nsDBFolderInfo::GetMailboxName(nsString *boxName)
{
	return GetPropertyWithToken(m_mailboxNameColumnToken, boxName);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumNewMessages(PRInt32 delta)
{
	m_numNewMessages += delta;
	if (m_numNewMessages < 0)
	{
#ifdef DEBUG_bienvenu1
		XP_ASSERT(FALSE);
#endif
		m_numNewMessages = 0;
	}
	return SetUint32PropertyWithToken(m_numNewMessagesColumnToken, m_numNewMessages);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumMessages(PRInt32 delta)
{
	m_numMessages += delta;
	if (m_numMessages < 0)
	{
#ifdef DEBUG_bienvenu
		NS_ASSERTION(PR_FALSE, "num messages can't be < 0");
#endif
		m_numMessages = 0;
	}
	return SetUint32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumVisibleMessages(PRInt32 delta)
{
	m_numVisibleMessages += delta;
	if (m_numVisibleMessages < 0)
	{
#ifdef DEBUG_bienvenu
		NS_ASSERTION(PR_FALSE, "num visible messages can't be < 0");
#endif
		m_numVisibleMessages = 0;
	}
	return SetUint32PropertyWithToken(m_numVisibleMessagesColumnToken, m_numVisibleMessages);
}

NS_IMETHODIMP nsDBFolderInfo::GetNumNewMessages(PRInt32 *result) 
{
	*result = m_numNewMessages;
	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetNumNewMessages(PRInt32 numNewMessages) 
{
	m_numNewMessages = numNewMessages;
	return SetUint32PropertyWithToken(m_numNewMessagesColumnToken, m_numNewMessages);
}

NS_IMETHODIMP	nsDBFolderInfo::GetNumMessages(PRInt32 *result) 
{
	*result = m_numMessages;
	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetNumMessages(PRInt32 numMessages) 
{
	m_numMessages = numMessages;
	return SetUint32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
}

NS_IMETHODIMP	nsDBFolderInfo::GetNumVisibleMessages(PRInt32 *result) 
{
	*result = m_numVisibleMessages;
	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetNumVisibleMessages(PRInt32 numVisibleMessages) 
{
	m_numVisibleMessages = numVisibleMessages;
	return SetUint32PropertyWithToken(m_numVisibleMessagesColumnToken, m_numVisibleMessages);
}

NS_IMETHODIMP nsDBFolderInfo::GetExpungedBytes(PRInt32 *result) 
{
	*result = m_expungedBytes;
	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetExpungedBytes(PRInt32 expungedBytes) 
{
	m_expungedBytes = expungedBytes;
	return SetUint32PropertyWithToken(m_expungedBytesColumnToken, m_expungedBytes);
}


NS_IMETHODIMP	nsDBFolderInfo::GetFlags(PRInt32 *result)
{
	*result = m_flags;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetFlags(PRInt32 flags)
{
	nsresult ret = NS_OK;

	if (m_flags != flags)
	{
    NS_ASSERTION((m_flags & MSG_FOLDER_FLAG_INBOX) == 0 || (flags & MSG_FOLDER_FLAG_INBOX) != 0, "lost inbox flag");
		m_flags = flags; 
		ret = SetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
	}
	return ret;
}

NS_IMETHODIMP nsDBFolderInfo::OrFlags(PRInt32 flags, PRInt32 *result)
{
	m_flags |= flags;
	*result = m_flags;
	return SetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
}

NS_IMETHODIMP nsDBFolderInfo::AndFlags(PRInt32 flags, PRInt32 *result)
{
	m_flags &= flags;
	*result = m_flags;
	return SetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
}

NS_IMETHODIMP	nsDBFolderInfo::GetImapUidValidity(PRInt32 *result) 
{
	*result = m_ImapUidValidity;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetImapUidValidity(PRInt32 uidValidity) 
{
	m_ImapUidValidity = uidValidity;
	return SetUint32PropertyWithToken(m_imapUidValidityColumnToken, m_ImapUidValidity);
}



NS_IMETHODIMP nsDBFolderInfo::GetLastMessageLoaded(nsMsgKey *lastLoaded) 
{
	*lastLoaded = m_lastMessageLoaded;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetLastMessageLoaded(nsMsgKey lastLoaded) 
{
	m_lastMessageLoaded = lastLoaded;
	return SetUint32PropertyWithToken(m_lastMessageLoadedColumnToken, m_lastMessageLoaded);
}

PRBool nsDBFolderInfo::TestFlag(PRInt32 flags)
{
	return (m_flags & flags) != 0;
}

NS_IMETHODIMP
nsDBFolderInfo::GetCharacterSet(nsString *result, PRBool *usedDefault) 
{
	nsresult rv = GetProperty(kCharacterSetColumnName, result);
	
	*usedDefault = PR_FALSE;

	if (NS_SUCCEEDED(rv) && result->IsEmpty())
	{
		result->Assign(gDefaultCharacterSet.get());
		*usedDefault = PR_TRUE;
	}

	return rv;
}

NS_IMETHODIMP
nsDBFolderInfo::GetCharPtrCharacterSet(char **result)
{
    nsresult rv = GetCharPtrProperty(kCharacterSetColumnName, result);

    if (NS_SUCCEEDED(rv) && (*result == nsnull || **result == '\0'))
    {
        *result = gDefaultCharacterSet.ToNewCString();
    }

    return rv;
}

NS_IMETHODIMP nsDBFolderInfo::SetCharacterSet(nsString *charSet) 
{
	return SetProperty(kCharacterSetColumnName, charSet);
}

NS_IMETHODIMP nsDBFolderInfo::GetCharacterSetOverride(PRBool *characterSetOverride) 
{
  *characterSetOverride = gDefaultCharacterOverride;
  PRUint32 propertyValue;
  nsresult rv = GetUint32Property(kCharacterSetOverrideColumnName, &propertyValue, gDefaultCharacterOverride);
  if (NS_SUCCEEDED(rv))
    *characterSetOverride = propertyValue;
  return rv;
}

NS_IMETHODIMP nsDBFolderInfo::SetCharacterSetOverride(PRBool characterSetOverride) 
{
  return SetUint32Property(kCharacterSetOverrideColumnName, characterSetOverride);
}

NS_IMETHODIMP
nsDBFolderInfo::GetLocale(nsString *result) 
{
	GetProperty(kLocaleColumnName, result);
    return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetLocale(nsString *locale) 
{
	return SetProperty(kLocaleColumnName, locale);
}


NS_IMETHODIMP nsDBFolderInfo::GetIMAPHierarchySeparator(PRUnichar *hierarchySeparator) 
{
	if (!hierarchySeparator)
		return NS_ERROR_NULL_POINTER;
	*hierarchySeparator = m_IMAPHierarchySeparator;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetIMAPHierarchySeparator(PRUnichar hierarchySeparator) 
{
	m_IMAPHierarchySeparator = hierarchySeparator; 
	return NS_OK;
}

NS_IMETHODIMP
nsDBFolderInfo::GetImapTotalPendingMessages(PRInt32 *result) 
{
    if (!result)
		return NS_ERROR_NULL_POINTER;
	*result = m_totalPendingMessages;
    return NS_OK;
}

void nsDBFolderInfo::ChangeImapTotalPendingMessages(PRInt32 delta)
{
	m_totalPendingMessages+=delta;
	SetInt32PropertyWithToken(m_totalPendingMessagesColumnToken, m_totalPendingMessages);
}

NS_IMETHODIMP
nsDBFolderInfo::GetImapUnreadPendingMessages(PRInt32 *result) 
{
    if (!result) 
		return NS_ERROR_NULL_POINTER;
	*result = m_unreadPendingMessages;
    return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetImapUnreadPendingMessages(PRInt32 numUnreadPendingMessages) 
{
	m_unreadPendingMessages = numUnreadPendingMessages;
	return SetUint32PropertyWithToken(m_unreadPendingMessagesColumnToken, m_unreadPendingMessages);
}

NS_IMETHODIMP	nsDBFolderInfo::SetImapTotalPendingMessages(PRInt32 numTotalPendingMessages) 
{
	m_totalPendingMessages = numTotalPendingMessages;
	return SetUint32PropertyWithToken(m_totalPendingMessagesColumnToken, m_totalPendingMessages);
}



void nsDBFolderInfo::ChangeImapUnreadPendingMessages(PRInt32 delta) 
{
	m_unreadPendingMessages+=delta;
	SetInt32PropertyWithToken(m_unreadPendingMessagesColumnToken, m_unreadPendingMessages);
}

/* attribute nsMsgViewTypeValue viewType; */
NS_IMETHODIMP nsDBFolderInfo::GetViewType(nsMsgViewTypeValue *aViewType)
{
  PRUint32 viewTypeValue;
  nsresult rv = GetUint32Property("viewType", &viewTypeValue, nsMsgViewType::eShowAllThreads);
  *aViewType = viewTypeValue;
  return rv;
}
NS_IMETHODIMP nsDBFolderInfo::SetViewType(nsMsgViewTypeValue aViewType)
{
  return SetUint32Property("viewType", aViewType);
}

/* attribute nsMsgViewFlagsTypeValue viewFlags; */
NS_IMETHODIMP nsDBFolderInfo::GetViewFlags(nsMsgViewFlagsTypeValue *aViewFlags)
{
  nsMsgViewFlagsTypeValue defaultViewFlags;
  nsresult rv = m_mdb->GetDefaultViewFlags(&defaultViewFlags);
  NS_ENSURE_SUCCESS(rv,rv);

  PRUint32 viewFlagsValue;
  rv = GetUint32Property("viewFlags", &viewFlagsValue, defaultViewFlags);
  *aViewFlags = viewFlagsValue;
  return rv;
}
NS_IMETHODIMP nsDBFolderInfo::SetViewFlags(nsMsgViewFlagsTypeValue aViewFlags)
{
  return SetUint32Property("viewFlags", aViewFlags);
}

/* attribute nsMsgViewSortTypeValue sortType; */
NS_IMETHODIMP nsDBFolderInfo::GetSortType(nsMsgViewSortTypeValue *aSortType)
{
  nsMsgViewSortTypeValue defaultSortType;
  nsresult rv = m_mdb->GetDefaultSortType(&defaultSortType);
  NS_ENSURE_SUCCESS(rv,rv);

  PRUint32 sortTypeValue;
  rv = GetUint32Property("sortType", &sortTypeValue, defaultSortType);
  *aSortType = sortTypeValue;
  return rv;
}
NS_IMETHODIMP nsDBFolderInfo::SetSortType(nsMsgViewSortTypeValue aSortType)
{
  return SetUint32Property("sortType", aSortType);
}

/* attribute nsMsgViewSortOrderValue sortOrder; */
NS_IMETHODIMP nsDBFolderInfo::GetSortOrder(nsMsgViewSortOrderValue *aSortOrder)
{
  PRUint32 sortOrderValue;
  nsresult rv = GetUint32Property("sortOrder",  &sortOrderValue, nsMsgViewSortOrder::ascending);
  *aSortOrder = sortOrderValue;
  return rv;
}

NS_IMETHODIMP nsDBFolderInfo::SetSortOrder(nsMsgViewSortOrderValue aSortOrder)
{
  return SetUint32Property("sortOrder", aSortOrder);
}


NS_IMETHODIMP nsDBFolderInfo::SetKnownArtsSet(nsString *newsArtSet)
{
	return SetProperty(kKnownArtsSetColumnName, newsArtSet);
}

NS_IMETHODIMP nsDBFolderInfo::GetKnownArtsSet(nsString *newsArtSet)
{
	return GetProperty(kKnownArtsSetColumnName, newsArtSet);
}

	// get arbitrary property, aka row cell value.

NS_IMETHODIMP	nsDBFolderInfo::GetProperty(const char *propertyName, nsString *resultProperty)
{
  return m_mdb->GetPropertyAsNSString(m_mdbRow, propertyName, resultProperty);
}

// Caller must PR_FREEIF resultProperty.
NS_IMETHODIMP	nsDBFolderInfo::GetCharPtrProperty(const char *propertyName, char **resultProperty)
{
  return m_mdb->GetProperty(m_mdbRow, propertyName, resultProperty);
}


NS_IMETHODIMP	nsDBFolderInfo::SetUint32Property(const char *propertyName, PRUint32 propertyValue)
{
  return m_mdb->SetUint32Property(m_mdbRow, propertyName, propertyValue);
}

NS_IMETHODIMP	nsDBFolderInfo::SetProperty(const char *propertyName, nsString *propertyStr)
{
  return m_mdb->SetPropertyFromNSString(m_mdbRow, propertyName, propertyStr);
}

nsresult nsDBFolderInfo::SetPropertyWithToken(mdb_token aProperty, nsString *propertyStr)
{
  return m_mdb->SetNSStringPropertyWithToken(m_mdbRow, aProperty, propertyStr);
}

nsresult	nsDBFolderInfo::SetUint32PropertyWithToken(mdb_token aProperty, PRUint32 propertyValue)
{
  return m_mdb->UInt32ToRowCellColumn(m_mdbRow, aProperty, propertyValue);
}

nsresult	nsDBFolderInfo::SetInt32PropertyWithToken(mdb_token aProperty, PRInt32 propertyValue)
{
	nsString propertyStr;
	propertyStr.AppendInt(propertyValue, 16);
	return SetPropertyWithToken(aProperty, &propertyStr);
}

nsresult nsDBFolderInfo::GetPropertyWithToken(mdb_token aProperty, nsString *resultProperty)
{
    if (!resultProperty)
		return NS_ERROR_NULL_POINTER;
	return m_mdb->RowCellColumnTonsString(m_mdbRow, aProperty, *resultProperty);
}

nsresult nsDBFolderInfo::GetUint32PropertyWithToken(mdb_token aProperty, PRUint32 &propertyValue, PRUint32 defaultValue)
{
	return m_mdb->RowCellColumnToUInt32(m_mdbRow, aProperty, propertyValue, defaultValue);
}

nsresult nsDBFolderInfo::GetInt32PropertyWithToken(mdb_token aProperty, PRInt32 &propertyValue, PRInt32 defaultValue)
{
	return m_mdb->RowCellColumnToUInt32(m_mdbRow, aProperty, (PRUint32 &) propertyValue, defaultValue);
}

NS_IMETHODIMP nsDBFolderInfo::GetUint32Property(const char *propertyName, PRUint32 *propertyValue, PRUint32 defaultValue)
{
  return m_mdb->GetUint32Property(m_mdbRow, propertyName, propertyValue, defaultValue);
}

NS_IMETHODIMP nsDBFolderInfo::GetBooleanProperty(const char *propertyName, PRBool *propertyValue, PRBool defaultValue)
{
  PRUint32 defaultUint32Value = (defaultValue) ? 1 : 0;
  PRUint32 returnValue;
  nsresult rv = m_mdb->GetUint32Property(m_mdbRow, propertyName, &returnValue, defaultUint32Value);
  *propertyValue = (returnValue != 0);
  return rv;
}
NS_IMETHODIMP	nsDBFolderInfo::SetBooleanProperty(const char *propertyName, PRBool propertyValue)
{
  return m_mdb->SetUint32Property(m_mdbRow, propertyName, propertyValue ? 1 : 0);
}


class nsTransferDBFolderInfo : public nsDBFolderInfo
{
public:
  nsTransferDBFolderInfo();
  virtual ~nsTransferDBFolderInfo();
  NS_IMETHOD GetMailboxName(nsString *boxName);
  NS_IMETHOD SetMailboxName(nsString *boxName);
  NS_IMETHOD GetViewType(nsMsgViewTypeValue *aViewType); 
  NS_IMETHOD SetViewType(nsMsgViewTypeValue aViewType); 
  NS_IMETHOD GetViewFlags(nsMsgViewFlagsTypeValue *aViewFlags); 
  NS_IMETHOD SetViewFlags(nsMsgViewFlagsTypeValue aViewFlags); 
  NS_IMETHOD GetSortType(nsMsgViewSortTypeValue *aSortType); 
  NS_IMETHOD SetSortType(nsMsgViewSortTypeValue aSortType); 
  NS_IMETHOD GetSortOrder(nsMsgViewSortOrderValue *aSortOrder); 
  NS_IMETHOD SetSortOrder(nsMsgViewSortOrderValue aSortOrder); 
  nsString  m_boxName;
  nsMsgViewTypeValue m_viewType;
  nsMsgViewFlagsTypeValue m_viewFlags;
  nsMsgViewSortTypeValue m_sortType;
  nsMsgViewSortOrderValue m_sortOrder;

};

nsTransferDBFolderInfo::nsTransferDBFolderInfo() : nsDBFolderInfo(nsnull)
{
}

nsTransferDBFolderInfo::~nsTransferDBFolderInfo()
{
}

/* void GetTransferInfo (out nsIDBFolderInfo transferInfo); */
NS_IMETHODIMP nsDBFolderInfo::GetTransferInfo(nsIDBFolderInfo **transferInfo)
{
  NS_ENSURE_ARG_POINTER(transferInfo);
  nsAutoString folderNameStr;

  nsTransferDBFolderInfo *newInfo = new nsTransferDBFolderInfo;
  *transferInfo = newInfo;
  NS_ADDREF(newInfo);
  newInfo->m_flags = m_flags;
  GetMailboxName(&folderNameStr);
  newInfo->SetMailboxName(&folderNameStr);
  // ### add whatever other fields we want to copy here.
  nsMsgViewTypeValue viewType;
  nsMsgViewFlagsTypeValue viewFlags;
  nsMsgViewSortTypeValue sortType;
  nsMsgViewSortOrderValue sortOrder;

  GetViewType(&viewType);
  GetViewFlags(&viewFlags);
  GetSortType(&sortType);
  GetSortOrder(&sortOrder);
  newInfo->SetViewType(viewType);
  newInfo->SetViewFlags(viewFlags);
  newInfo->SetSortType(sortType);
  newInfo->SetSortOrder(sortOrder);
  return NS_OK;
}

NS_IMETHODIMP nsTransferDBFolderInfo::GetMailboxName(nsString *boxName)
{
  *boxName = m_boxName;
  return NS_OK;
}

NS_IMETHODIMP nsTransferDBFolderInfo::SetMailboxName(nsString *boxName)
{
  m_boxName = *boxName;
  return NS_OK;
}

NS_IMPL_GETSET(nsTransferDBFolderInfo, ViewType, nsMsgViewTypeValue, m_viewType);
NS_IMPL_GETSET(nsTransferDBFolderInfo, ViewFlags, nsMsgViewFlagsTypeValue, m_viewFlags);
NS_IMPL_GETSET(nsTransferDBFolderInfo, SortType, nsMsgViewSortTypeValue, m_sortType);
NS_IMPL_GETSET(nsTransferDBFolderInfo, SortOrder, nsMsgViewSortOrderValue, m_sortOrder);

/* void InitFromTransferInfo (in nsIDBFolderInfo transferInfo); */
NS_IMETHODIMP nsDBFolderInfo::InitFromTransferInfo(nsIDBFolderInfo *transferInfo)
{
  NS_ENSURE_ARG(transferInfo);
  PRInt32 flags;
  nsAutoString folderNameStr;

  transferInfo->GetFlags(&flags);
  SetFlags(flags);
  transferInfo->GetMailboxName(&folderNameStr);
  SetMailboxName(&folderNameStr);
  // ### add whatever other fields we want to copy here.

  nsMsgViewTypeValue viewType;
  nsMsgViewFlagsTypeValue viewFlags;
  nsMsgViewSortTypeValue sortType;
  nsMsgViewSortOrderValue sortOrder;

  transferInfo->GetViewType(&viewType);
  transferInfo->GetViewFlags(&viewFlags);
  transferInfo->GetSortType(&sortType);
  transferInfo->GetSortOrder(&sortOrder);
  SetViewType(viewType);
  SetViewFlags(viewFlags);
  SetSortType(sortType);
  SetSortOrder(sortOrder);
  return NS_OK;
}

