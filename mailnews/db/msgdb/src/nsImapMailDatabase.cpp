/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999-2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef XP_MAC
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#include "msgCore.h"
#include "nsImapMailDatabase.h"
#include "nsDBFolderInfo.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsIFileSpec.h"

const char *kPendingHdrsScope = "ns:msg:db:row:scope:pending:all";	// scope for all offine ops table
const char *kPendingHdrsTableKind = "ns:msg:db:table:kind:pending";
struct mdbOid gAllPendingHdrsTableOID;

nsImapMailDatabase::nsImapMailDatabase()
{
  m_mdbAllPendingHdrsTable = nsnull;
}

nsImapMailDatabase::~nsImapMailDatabase()
{
}

NS_IMETHODIMP nsImapMailDatabase::Open(nsIFileSpec *aFolderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB)
{
  nsImapMailDatabase	*mailDB;
  PRBool                summaryFileExists;
  struct stat           st;
  PRBool                newFile = PR_FALSE;
#ifdef DEBUG_bienvenu
  NS_ASSERTION(m_folder, "folder should be set");
#endif
  if (!aFolderName)
    return NS_ERROR_NULL_POINTER;
  
  nsFileSpec		folderName;
  aFolderName->GetFileSpec(&folderName);
  
  nsLocalFolderSummarySpec	summarySpec(folderName);
  
  nsIDBFolderInfo	*folderInfo = NULL;
  
  *pMessageDB = NULL;
  
  nsFileSpec dbPath(summarySpec);
  
  mailDB = (nsImapMailDatabase *) FindInCache(dbPath);
  if (mailDB)
  {
    *pMessageDB = mailDB;
    return(NS_OK);
  }
  
#if defined(DEBUG_bienvenu) || defined(DEBUG_jefft)
  printf("really opening db in nsImapMailDatabase::Open(%s, %s, %p, %s) -> %s\n",
    (const char*)folderName, create ? "TRUE":"FALSE",
    pMessageDB, upgrading ? "TRUE":"FALSE", (const char*)folderName);
#endif
  // if the old summary doesn't exist, we're creating a new one.
  if (!summarySpec.Exists() && create)
    newFile = PR_TRUE;
  
  mailDB = new nsImapMailDatabase;
  
  if (!mailDB)
    return NS_ERROR_OUT_OF_MEMORY;
  mailDB->m_folderSpec = new nsFileSpec(folderName);
  mailDB->m_folder = m_folder;
  mailDB->AddRef();
  // stat file before we open the db, because if we've latered
  // any messages, handling latered will change time stamp on
  // folder file.
  summaryFileExists = summarySpec.Exists();
  
  char	*nativeFolderName = nsCRT::strdup((const char *) folderName);
  
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_MAC)
  UnixToNative(nativeFolderName);
#endif
  stat (nativeFolderName, &st);
  PR_FREEIF(nativeFolderName);
  
  nsresult err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
  
  err = mailDB->OpenMDB((const char *) summarySpec, create);
  
  if (err == NS_OK)
  {
    mailDB->GetDBFolderInfo(&folderInfo);
    if (folderInfo == NULL)
    {
      err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
    }
    else
    {
      // compare current version of db versus filed out version info.
      PRUint32 version;
      folderInfo->GetVersion(&version);
      if (mailDB->GetCurVersion() != version)
        err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
      NS_RELEASE(folderInfo);
    }
    if (err != NS_OK)
    {
      // this will make the db folder info release its ref to the mail db...
      NS_IF_RELEASE(mailDB->m_dbFolderInfo);
      mailDB->ForceClosed();
      if (err == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
        summarySpec.Delete(PR_FALSE);
      
      mailDB = NULL;
    }
  }
  if (err != NS_OK || newFile)
  {
    // if we couldn't open file, or we have a blank one, and we're supposed 
    // to upgrade, updgrade it.
    if (newFile && !upgrading)	// caller is upgrading, and we have empty summary file,
    {					// leave db around and open so caller can upgrade it.
      err = NS_MSG_ERROR_FOLDER_SUMMARY_MISSING;
    }
    else if (err != NS_OK)
    {
      *pMessageDB = NULL;
      if (mailDB)
        mailDB->Close(PR_FALSE);
      delete mailDB;
      summarySpec.Delete(PR_FALSE);  // blow away the db if it's corrupt.
      mailDB = NULL;
    }
  }
  if (err == NS_OK || err == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
  {
    *pMessageDB = mailDB;
    if (mailDB)
      GetDBCache()->AppendElement(mailDB);
    
  }
  return err;
}

NS_IMETHODIMP	nsImapMailDatabase::SetSummaryValid(PRBool /* valid */)
{
  return NS_OK;
}

// IMAP does not set local file flags, override does nothing
void	nsImapMailDatabase::UpdateFolderFlag(nsIMsgDBHdr * /* msgHdr */, PRBool /* bSet */, 
                                              MsgFlags /* flag */, nsIOFileStream ** /* ppFileStream */)
{
}

PRBool nsImapMailDatabase::SetHdrFlag(nsIMsgDBHdr *msgHdr, PRBool bSet, MsgFlags flag)
{
  return nsMsgDatabase::SetHdrFlag(msgHdr, bSet, flag);
}

// override so nsMailDatabase methods that deal with m_folderStream are *not* called
NS_IMETHODIMP nsImapMailDatabase::StartBatch()
{
  return NS_OK;
}

NS_IMETHODIMP nsImapMailDatabase::EndBatch()
{
  return NS_OK;
}

NS_IMETHODIMP nsImapMailDatabase::DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator)
{
  return nsMsgDatabase::DeleteMessages(nsMsgKeys, instigator);
}

nsresult nsImapMailDatabase::AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr)
{
  PRUint32 msgFlags;
  msgHdr->GetFlags(&msgFlags);
  if (msgFlags & MSG_FLAG_OFFLINE && m_dbFolderInfo)
  {
    PRUint32 size = 0;
    (void)msgHdr->GetOfflineMessageSize(&size);
    return m_dbFolderInfo->ChangeExpungedBytes (size);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapMailDatabase::ForceClosed()
{
  m_mdbAllPendingHdrsTable = nsnull;
  return nsMailDatabase::ForceClosed();
}

NS_IMETHODIMP nsImapMailDatabase::SetFolderStream(nsIOFileStream *aFileStream)
{
  NS_ASSERTION(0, "Trying to set folderStream, not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsImapMailDatabase::GetAllPendingHdrsTable()
{
  nsresult rv = NS_OK;
  if (!m_mdbAllPendingHdrsTable)
    rv = GetTableCreateIfMissing(kPendingHdrsScope, kPendingHdrsTableKind, getter_AddRefs(m_mdbAllPendingHdrsTable),
                                                m_pendingHdrsRowScopeToken, m_pendingHdrsTableKindToken) ;
  return rv;
}

static const char *kFlagsName = "flags";

NS_IMETHODIMP nsImapMailDatabase::AddNewHdrToDB(nsIMsgDBHdr *newHdr, PRBool notify)
{
  nsresult rv = nsMsgDatabase::AddNewHdrToDB(newHdr, notify);
  if (NS_SUCCEEDED(rv))
  {
    rv = GetAllPendingHdrsTable();
    NS_ENSURE_SUCCESS(rv, rv);
    mdb_count numPendingHdrs = 0;
    m_mdbAllPendingHdrsTable->GetCount(GetEnv(), &numPendingHdrs);
    if (numPendingHdrs > 0)
    {
      mdbYarn messageIdYarn;
      nsCOMPtr <nsIMdbRow> pendingRow;
      mdbOid  outRowId;
  
      nsXPIDLCString messageId;
      newHdr->GetMessageId(getter_Copies(messageId));
      messageIdYarn.mYarn_Buf = (void*)messageId.get();
      messageIdYarn.mYarn_Fill = messageId.Length();
      messageIdYarn.mYarn_Form = 0;
      messageIdYarn.mYarn_Size = messageIdYarn.mYarn_Fill;
  
      m_mdbStore->FindRow(GetEnv(), m_pendingHdrsRowScopeToken, 
                m_messageIdColumnToken, &messageIdYarn, &outRowId, getter_AddRefs(pendingRow));
      if (pendingRow)
      {
        mdb_count numCells;
        mdbYarn cellYarn;
        mdb_column cellColumn;

        pendingRow->GetCount(GetEnv(), &numCells);
        // iterate over the cells in the pending hdr setting properties on the newHdr.
        // we skip cell 0, which is the messageId;
        for (mdb_count cellIndex = 1; cellIndex < numCells; cellIndex++)
        {
          mdb_err err = pendingRow->SeekCellYarn(GetEnv(), cellIndex, &cellColumn, nsnull);
          if (err == 0)
          {
            err = pendingRow->AliasCellYarn(GetEnv(), cellColumn, &cellYarn);
            if (err == 0)
            {
              nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, newHdr);      // closed system, cast ok
              nsIMdbRow *row = msgHdr->GetMDBRow();
              if (row)
                row->AddColumn(GetEnv(), cellColumn, &cellYarn);
            }
          }
        }
        m_mdbAllPendingHdrsTable->CutRow(GetEnv(), pendingRow);
        pendingRow->CutAllColumns(GetEnv());
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailDatabase::SetAttributesOnPendingHdr(nsIMsgDBHdr *pendingHdr, const char *property, 
                                  const char *propertyVal, PRInt32 flags)
{
  NS_ENSURE_ARG_POINTER(pendingHdr);
  nsresult rv = GetAllPendingHdrsTable();
  NS_ENSURE_SUCCESS(rv, rv);

  mdbYarn messageIdYarn;
  nsCOMPtr <nsIMdbRow> pendingRow;
  nsIMdbRow *newHdrRow;
  mdbOid  outRowId;
  mdb_err err;
  nsXPIDLCString messageId;
  pendingHdr->GetMessageId(getter_Copies(messageId));
  messageIdYarn.mYarn_Buf = (void*)messageId.get();
  messageIdYarn.mYarn_Fill = messageId.Length();
  messageIdYarn.mYarn_Form = 0;
  messageIdYarn.mYarn_Size = messageIdYarn.mYarn_Fill;

  err = m_mdbStore->FindRow(GetEnv(), m_pendingHdrsRowScopeToken, 
            m_messageIdColumnToken, &messageIdYarn, &outRowId, getter_AddRefs(pendingRow));

  if (!pendingRow)
  {
    err  = m_mdbStore->NewRow(GetEnv(), m_pendingHdrsRowScopeToken, &newHdrRow);
    pendingRow = do_QueryInterface(newHdrRow);
  }
  NS_ENSURE_SUCCESS(err, err);
  if (pendingRow)
  {
    // now we need to add cells to the row to remember the messageid, property and property value, and flags. 
    // Then, when hdrs are added to the db, we'll check if they have a matching message-id, and if so,
    // set the property and flags
    nsXPIDLCString messageId;
    pendingHdr->GetMessageId(getter_Copies(messageId));
    // we're just going to ignore messages without a message-id. They should be rare. If SPAM messages often
    // didn't have message-id's, they'd be filtered on the server, most likely, and spammers would then
    // start putting in message-id's.
    if (!messageId.IsEmpty())
    {
       extern const char *kMessageIdColumnName;
       m_mdbAllPendingHdrsTable->AddRow(GetEnv(), pendingRow);
       // make sure this is the first cell so that when we ignore the first
       // cell in nsImapMailDatabase::AddNewHdrToDB, we're ignoring the right one
      (void) SetProperty(pendingRow, kMessageIdColumnName, messageId.get());
      (void) SetProperty(pendingRow, property, propertyVal);
      (void) SetUint32Property(pendingRow, kFlagsName, (PRUint32) flags);
    }
    else 
      return NS_ERROR_FAILURE; 
  }
  return rv;
}

