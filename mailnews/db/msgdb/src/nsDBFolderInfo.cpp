/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsDBFolderInfo.h"
#include "nsMsgDatabase.h"
#include "nsMsgFolderFlags.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIMsgDBView.h"
#include "nsReadableUtils.h"
#include "nsISupportsObsolete.h"

static const char *kDBFolderInfoScope = "ns:msg:db:row:scope:dbfolderinfo:all";
static const char *kDBFolderInfoTableKind = "ns:msg:db:table:kind:dbfolderinfo";

struct mdbOid gDBFolderInfoOID;

static const char * kNumMessagesColumnName ="numMsgs";
// have to leave this as numNewMsgs even though it's numUnread Msgs
static const char * kNumUnreadMessagesColumnName = "numNewMsgs"; 
static const char * kFlagsColumnName = "flags";
static const char * kFolderSizeColumnName = "folderSize";
static const char * kExpungedBytesColumnName = "expungedBytes";
static const char * kFolderDateColumnName = "folderDate";
static const char * kHighWaterMessageKeyColumnName = "highWaterKey";

static const char * kImapUidValidityColumnName = "UIDValidity";
static const char * kTotalPendingMessagesColumnName = "totPendingMsgs";
static const char * kUnreadPendingMessagesColumnName = "unreadPendingMsgs";
static const char * kMailboxNameColumnName = "mailboxName";
static const char * kKnownArtsSetColumnName = "knownArts";
static const char * kExpiredMarkColumnName = "expiredMark";
static const char * kVersionColumnName = "version";
static const char * kCharacterSetColumnName = "charSet";
static const char * kCharacterSetOverrideColumnName = "charSetOverride";
static const char * kLocaleColumnName = "locale";


#define kMAILNEWS_VIEW_DEFAULT_CHARSET        "mailnews.view_default_charset"
#define kMAILNEWS_DEFAULT_CHARSET_OVERRIDE    "mailnews.force_charset_override"
static char * gDefaultCharacterSet = NULL;
static PRBool     gDefaultCharacterOverride;
static nsIObserver *gFolderCharsetObserver = nsnull;
static PRBool     gInitializeObserver = PR_FALSE;
static PRBool     gReleaseObserver = PR_FALSE;

// observer for charset related preference notification
class nsFolderCharsetObserver : public nsIObserver {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsFolderCharsetObserver() { }
  virtual ~nsFolderCharsetObserver() {}
};

NS_IMPL_ISUPPORTS1(nsFolderCharsetObserver, nsIObserver)

NS_IMETHODIMP nsFolderCharsetObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv;

  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID))
  {
    nsDependentString prefName(someData);
    
    if (prefName.EqualsLiteral(kMAILNEWS_VIEW_DEFAULT_CHARSET))
    {
      nsCOMPtr<nsIPrefLocalizedString> pls;
      rv = prefBranch->GetComplexValue(kMAILNEWS_VIEW_DEFAULT_CHARSET,
                      NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(pls));
      if (NS_SUCCEEDED(rv)) 
      {
        nsXPIDLString ucsval;
        pls->ToString(getter_Copies(ucsval));
        if (ucsval)
        {
          if (gDefaultCharacterSet)
            nsMemory::Free(gDefaultCharacterSet); 
          gDefaultCharacterSet = ToNewCString(ucsval);
        }
      }
    }
    else if (prefName.EqualsLiteral(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE))
    {
      rv = prefBranch->GetBoolPref(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, &gDefaultCharacterOverride);
    }
  }
  else if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
  {
    nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(prefBranch);
    if (pbi) 
    {
      rv = pbi->RemoveObserver(kMAILNEWS_VIEW_DEFAULT_CHARSET, this);
      rv = pbi->RemoveObserver(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, this);
    }
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
    iid.Equals(NS_GET_IID(nsISupports))) 
  {
    *result = NS_STATIC_CAST(nsIDBFolderInfo*, this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


nsDBFolderInfo::nsDBFolderInfo(nsMsgDatabase *mdb)
                  : m_flags(0),
                    m_expiredMark(0),
                    m_expiredMarkColumnToken(0)
{
  m_mdbTable = NULL;
  m_mdbRow = NULL;
  m_version = 1;			// for upgrading...
  m_IMAPHierarchySeparator = 0;	// imap path separator
  // mail only (for now)
  m_folderSize = 0;
  m_folderDate = 0;
  m_expungedBytes = 0;	// sum of size of deleted messages in folder
  m_highWaterMessageKey = 0;
  
  m_numUnreadMessages = 0;
  m_numMessages = 0;
  // IMAP only
  m_ImapUidValidity = 0;
  m_totalPendingMessages =0;
  m_unreadPendingMessages = 0;
  
  m_mdbTokensInitialized = PR_FALSE;
  m_charSetOverride = PR_FALSE;
  
  if (!gInitializeObserver)
  {
    gInitializeObserver = PR_TRUE;
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsIPrefBranch> prefBranch;
    if (NS_SUCCEEDED(rv))
    {
      rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    }
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIPrefLocalizedString> pls;
      rv = prefBranch->GetComplexValue(kMAILNEWS_VIEW_DEFAULT_CHARSET,
        NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(pls));
      if (NS_SUCCEEDED(rv)) 
      {
        nsXPIDLString ucsval;
        pls->ToString(getter_Copies(ucsval));
        if (ucsval)
        {
          if (gDefaultCharacterSet)
            nsMemory::Free(gDefaultCharacterSet);
          gDefaultCharacterSet = ToNewCString(ucsval);
        }
      }
      rv = prefBranch->GetBoolPref(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, &gDefaultCharacterOverride);
      
      gFolderCharsetObserver = new nsFolderCharsetObserver();
      NS_ASSERTION(gFolderCharsetObserver, "failed to create observer");
      
      // register prefs callbacks
      if (gFolderCharsetObserver)
      {
        NS_ADDREF(gFolderCharsetObserver);
        nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(prefBranch);
        if (pbi) {
          rv = pbi->AddObserver(kMAILNEWS_VIEW_DEFAULT_CHARSET, gFolderCharsetObserver, PR_FALSE);
          rv = pbi->AddObserver(kMAILNEWS_DEFAULT_CHARSET_OVERRIDE, gFolderCharsetObserver, PR_FALSE);
        }
        
        // also register for shutdown
        nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
        if (NS_SUCCEEDED(rv))
        {
          rv = observerService->AddObserver(gFolderCharsetObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
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
  // nsMsgDatabase strictly owns nsDBFolderInfo, so don't ref-count db.
  ReleaseExternalReferences();
}

// Release any objects we're holding onto. This needs to be safe
// to call multiple times.
void nsDBFolderInfo::ReleaseExternalReferences()
{
  if (gReleaseObserver && gFolderCharsetObserver) 
  {
    NS_IF_RELEASE(gFolderCharsetObserver);
  
    // this can be called many times
    if (gDefaultCharacterSet)
    {
      nsMemory::Free(gDefaultCharacterSet);
      gDefaultCharacterSet = NULL; // free doesn't null out our ptr.
    }
  }
  
  if (m_mdb)
  {
    if (m_mdbTable)
    {
      NS_RELEASE(m_mdbTable);
      m_mdbTable = nsnull;
    }
    if (m_mdbRow)
    {
      NS_RELEASE(m_mdbRow);
      m_mdbRow = nsnull;
    }
    m_mdb = nsnull;
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
            NS_RELEASE(rowCursor);
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
    
    store->StringToToken(env,  kNumMessagesColumnName, &m_numMessagesColumnToken);
    store->StringToToken(env,  kNumUnreadMessagesColumnName, &m_numUnreadMessagesColumnToken);
    store->StringToToken(env,  kFlagsColumnName, &m_flagsColumnToken);
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
  GetInt32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
  GetInt32PropertyWithToken(m_numUnreadMessagesColumnToken, m_numUnreadMessages);
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
  m_charSetOverride = gDefaultCharacterOverride;
  PRUint32 propertyValue;
  nsresult rv = GetUint32Property(kCharacterSetOverrideColumnName, gDefaultCharacterOverride, &propertyValue);
  if (NS_SUCCEEDED(rv))
    m_charSetOverride = propertyValue;

  nsXPIDLCString charSet;
  if (NS_SUCCEEDED(m_mdb->GetProperty(m_mdbRow, kCharacterSetColumnName, getter_Copies(charSet))))
    m_charSet = charSet;

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

NS_IMETHODIMP nsDBFolderInfo::SetMailboxName(const nsAString &newBoxName)
{
  return SetPropertyWithToken(m_mailboxNameColumnToken, newBoxName);
}

NS_IMETHODIMP nsDBFolderInfo::GetMailboxName(nsAString &boxName)
{
  return GetPropertyWithToken(m_mailboxNameColumnToken, boxName);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumUnreadMessages(PRInt32 delta)
{
  m_numUnreadMessages += delta;
  // m_numUnreadMessages can never be set to negative.
  if (m_numUnreadMessages < 0)
  {
#ifdef DEBUG_bienvenu1
     NS_ASSERTION(PR_FALSE, "Hardcoded assertion");
#endif
      m_numUnreadMessages = 0;
  }
  return SetUint32PropertyWithToken(m_numUnreadMessagesColumnToken, m_numUnreadMessages);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumMessages(PRInt32 delta)
{
  m_numMessages += delta;
  // m_numMessages can never be set to negative.
  if (m_numMessages < 0)
  {
#ifdef DEBUG_bienvenu
    NS_ASSERTION(PR_FALSE, "num messages can't be < 0");
#endif
    m_numMessages = 0;
  }
  return SetUint32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
}


NS_IMETHODIMP nsDBFolderInfo::GetNumUnreadMessages(PRInt32 *result) 
{
  *result = m_numUnreadMessages;
  return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetNumUnreadMessages(PRInt32 numUnreadMessages) 
{
  m_numUnreadMessages = numUnreadMessages;
  return SetUint32PropertyWithToken(m_numUnreadMessagesColumnToken, m_numUnreadMessages);
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

PRBool nsDBFolderInfo::TestFlag(PRInt32 flags)
{
  return (m_flags & flags) != 0;
}

NS_IMETHODIMP
nsDBFolderInfo::GetCharacterSet(nsACString &result, PRBool *usedDefault) 
{
  *usedDefault = PR_FALSE;

  nsXPIDLCString val;
  nsresult rv = GetCharPtrProperty(kCharacterSetColumnName, getter_Copies(val));
  result = val;
  
  if (NS_SUCCEEDED(rv) && result.IsEmpty())
  {
    result = gDefaultCharacterSet;
    *usedDefault = PR_TRUE;
  }
  
  return rv;
}

nsresult nsDBFolderInfo::GetConstCharPtrCharacterSet(const char**result)
{
  if (!m_charSet.IsEmpty())
    *result = m_charSet.get();
  else
    *result = gDefaultCharacterSet;
  return NS_OK;
}

NS_IMETHODIMP
nsDBFolderInfo::GetCharPtrCharacterSet(char **result)
{
  *result = ToNewCString(m_charSet);

  if ((*result == nsnull || **result == '\0'))
  {
    PR_Free(*result);
    *result = strdup(gDefaultCharacterSet);
  }

  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsDBFolderInfo::SetCharacterSet(const char *charSet) 
{
  m_charSet.Assign(charSet);
  return SetCharPtrProperty(kCharacterSetColumnName, charSet);
}

NS_IMETHODIMP nsDBFolderInfo::GetCharacterSetOverride(PRBool *characterSetOverride) 
{
  *characterSetOverride = m_charSetOverride;
  return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetCharacterSetOverride(PRBool characterSetOverride) 
{
  m_charSetOverride = characterSetOverride;
  return SetUint32Property(kCharacterSetOverrideColumnName, characterSetOverride);
}

NS_IMETHODIMP
nsDBFolderInfo::GetLocale(nsAString &result) 
{
  GetProperty(kLocaleColumnName, result);
  return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetLocale(const nsAString &locale) 
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
  nsresult rv = GetUint32Property("viewType", nsMsgViewType::eShowAllThreads, &viewTypeValue);
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
  rv = GetUint32Property("viewFlags", defaultViewFlags, &viewFlagsValue);
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
  rv = GetUint32Property("sortType", defaultSortType, &sortTypeValue);
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
  nsresult rv = GetUint32Property("sortOrder", nsMsgViewSortOrder::ascending, &sortOrderValue);
  *aSortOrder = sortOrderValue;
  return rv;
}

NS_IMETHODIMP nsDBFolderInfo::SetSortOrder(nsMsgViewSortOrderValue aSortOrder)
{
  return SetUint32Property("sortOrder", aSortOrder);
}


NS_IMETHODIMP nsDBFolderInfo::SetKnownArtsSet(const char *newsArtSet)
{
  return m_mdb->SetProperty(m_mdbRow, kKnownArtsSetColumnName, newsArtSet);
}

NS_IMETHODIMP nsDBFolderInfo::GetKnownArtsSet(char **newsArtSet)
{
  return m_mdb->GetProperty(m_mdbRow, kKnownArtsSetColumnName, newsArtSet);
}

// get arbitrary property, aka row cell value.
NS_IMETHODIMP nsDBFolderInfo::GetProperty(const char *propertyName, nsAString &resultProperty)
{
  return m_mdb->GetPropertyAsNSString(m_mdbRow, propertyName, resultProperty);
}

NS_IMETHODIMP nsDBFolderInfo::SetCharPtrProperty(const char *aPropertyName, const char *aPropertyValue)
{
  return m_mdb->SetProperty(m_mdbRow, aPropertyName, aPropertyValue);
}

// Caller must PR_Free resultProperty.
NS_IMETHODIMP nsDBFolderInfo::GetCharPtrProperty(const char *propertyName, char **resultProperty)
{
  return m_mdb->GetProperty(m_mdbRow, propertyName, resultProperty);
}


NS_IMETHODIMP nsDBFolderInfo::SetUint32Property(const char *propertyName, PRUint32 propertyValue)
{
  return m_mdb->SetUint32Property(m_mdbRow, propertyName, propertyValue);
}

NS_IMETHODIMP	nsDBFolderInfo::SetProperty(const char *propertyName, const nsAString &propertyStr)
{
  return m_mdb->SetPropertyFromNSString(m_mdbRow, propertyName, propertyStr);
}

nsresult nsDBFolderInfo::SetPropertyWithToken(mdb_token aProperty, const nsAString &propertyStr)
{
  return m_mdb->SetNSStringPropertyWithToken(m_mdbRow, aProperty, propertyStr);
}

nsresult  nsDBFolderInfo::SetUint32PropertyWithToken(mdb_token aProperty, PRUint32 propertyValue)
{
  return m_mdb->UInt32ToRowCellColumn(m_mdbRow, aProperty, propertyValue);
}

nsresult  nsDBFolderInfo::SetInt32PropertyWithToken(mdb_token aProperty, PRInt32 propertyValue)
{
  nsAutoString propertyStr;
  propertyStr.AppendInt(propertyValue, 16);
  return SetPropertyWithToken(aProperty, propertyStr);
}

nsresult nsDBFolderInfo::GetPropertyWithToken(mdb_token aProperty, nsAString &resultProperty)
{
  return m_mdb->RowCellColumnTonsString(m_mdbRow, aProperty, resultProperty);
}

nsresult nsDBFolderInfo::GetUint32PropertyWithToken(mdb_token aProperty, PRUint32 &propertyValue, PRUint32 defaultValue)
{
  return m_mdb->RowCellColumnToUInt32(m_mdbRow, aProperty, propertyValue, defaultValue);
}

nsresult nsDBFolderInfo::GetInt32PropertyWithToken(mdb_token aProperty, PRInt32 &propertyValue, PRInt32 defaultValue)
{
  return m_mdb->RowCellColumnToUInt32(m_mdbRow, aProperty, (PRUint32 &) propertyValue, defaultValue);
}

NS_IMETHODIMP nsDBFolderInfo::GetUint32Property(const char *propertyName, PRUint32 defaultValue, PRUint32 *propertyValue)
{
  return m_mdb->GetUint32Property(m_mdbRow, propertyName, propertyValue, defaultValue);
}

NS_IMETHODIMP nsDBFolderInfo::GetBooleanProperty(const char *propertyName, PRBool defaultValue, PRBool *propertyValue)
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

NS_IMETHODIMP nsDBFolderInfo::GetFolderName(char **folderName)
{
  return GetCharPtrProperty("folderName", folderName);
}

NS_IMETHODIMP nsDBFolderInfo::SetFolderName(const char *folderName)
{
  return SetCharPtrProperty("folderName", folderName);
}

class nsTransferDBFolderInfo : public nsDBFolderInfo
{
public:
  nsTransferDBFolderInfo();
  virtual ~nsTransferDBFolderInfo();
  // parallel arrays of properties and values
  nsCStringArray m_properties;
  nsCStringArray m_values;
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

  nsTransferDBFolderInfo *newInfo = new nsTransferDBFolderInfo;
  *transferInfo = newInfo;
  NS_ADDREF(newInfo);
  
  mdb_count numCells;
  mdbYarn cellYarn;
  mdb_column cellColumn;
  char columnName[100];
  mdbYarn cellName = { columnName, 0, sizeof(columnName), 0, 0, nsnull };

  m_mdbRow->GetCount(m_mdb->GetEnv(), &numCells);
  // iterate over the cells in the dbfolderinfo remembering attribute names and values.
  for (mdb_count cellIndex = 0; cellIndex < numCells; cellIndex++)
  {
    mdb_err err = m_mdbRow->SeekCellYarn(m_mdb->GetEnv(), cellIndex, &cellColumn, nsnull);
    if (!err)
    {
      err = m_mdbRow->AliasCellYarn(m_mdb->GetEnv(), cellColumn, &cellYarn);
      if (!err)
      {
        m_mdb->GetStore()->TokenToString(m_mdb->GetEnv(), cellColumn, &cellName);
        newInfo->m_values.AppendCString(Substring((const char *)cellYarn.mYarn_Buf, 
                                          (const char *) cellYarn.mYarn_Buf + cellYarn.mYarn_Fill));
        newInfo->m_properties.AppendCString(Substring((const char *) cellName.mYarn_Buf, 
                                          (const char *) cellName.mYarn_Buf + cellName.mYarn_Fill));
      }
    }
  }
  
  return NS_OK;
}


/* void InitFromTransferInfo (in nsIDBFolderInfo transferInfo); */
NS_IMETHODIMP nsDBFolderInfo::InitFromTransferInfo(nsIDBFolderInfo *aTransferInfo)
{
  NS_ENSURE_ARG(aTransferInfo);

  nsTransferDBFolderInfo *transferInfo = NS_STATIC_CAST(nsTransferDBFolderInfo *, aTransferInfo);

  for (PRInt32 i = 0; i < transferInfo->m_values.Count(); i++)
    SetCharPtrProperty(transferInfo->m_properties[i]->get(), transferInfo->m_values[i]->get());

  LoadMemberVariables();

  return NS_OK;
}

