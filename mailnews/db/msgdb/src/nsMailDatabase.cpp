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
#include "nsImapCore.h" // for imap flags
#include "nsMailDatabase.h"
#include "nsDBFolderInfo.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsFileStream.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsFileSpec.h"
#include "nsMsgOfflineImapOperation.h"
#include "nsMsgFolderFlags.h"
#include "prlog.h"
#include "prprf.h"
#include "nsIFileSpec.h"

const char *kOfflineOpsScope = "ns:msg:db:row:scope:ops:all";	// scope for all offine ops table
const char *kOfflineOpsTableKind = "ns:msg:db:table:kind:ops";
struct mdbOid gAllOfflineOpsTableOID;


nsMailDatabase::nsMailDatabase()
    : m_reparse(PR_FALSE), m_folderSpec(nsnull), m_folderStream(nsnull)
{
  m_mdbAllOfflineOpsTable = nsnull;
}

nsMailDatabase::~nsMailDatabase()
{
  if(m_folderSpec)
    delete m_folderSpec;
  if (m_mdbAllOfflineOpsTable)
    m_mdbAllOfflineOpsTable->Release();
}



NS_IMETHODIMP nsMailDatabase::Open(nsIFileSpec *aFolderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB)
{
  nsMailDatabase	*mailDB;
  PRBool		summaryFileExists;
  PRBool		newFile = PR_FALSE;
  PRBool                deleteInvalidDB = PR_FALSE;
  nsFileSpec		folderName;
  
  if (!aFolderName)
    return NS_ERROR_NULL_POINTER;
  aFolderName->GetFileSpec(&folderName);
  nsLocalFolderSummarySpec	summarySpec(folderName);
  
  nsIDBFolderInfo	*folderInfo = nsnull;
  
  *pMessageDB = nsnull;
  
  nsFileSpec dbPath(summarySpec);
  
  mailDB = (nsMailDatabase *) FindInCache(dbPath);
  if (mailDB)
  {
    *pMessageDB = mailDB;
    //FindInCache does the AddRef'ing
    return(NS_OK);
  }
  
  // if the old summary doesn't exist, we're creating a new one.
  if (!summarySpec.Exists() && create)
    newFile = PR_TRUE;
  
  mailDB = new nsMailDatabase();
  
  if (!mailDB)
    return NS_ERROR_OUT_OF_MEMORY;
  mailDB->m_folderSpec = new nsFileSpec(folderName);
  mailDB->m_folder = m_folder;
  mailDB->AddRef();
  // stat file before we open the db, because if we've latered
  // any messages, handling latered will change time stamp on
  // folder file.
  summaryFileExists = summarySpec.Exists();
  
  nsresult err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
  
  err = mailDB->OpenMDB((const char *) summarySpec, create);
  
  if (NS_SUCCEEDED(err))
  {
    mailDB->GetDBFolderInfo(&folderInfo);
    if (folderInfo == NULL)
    {
      err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
    }
    else
    {
      // if opening existing file, make sure summary file is up to date.
      // if caller is upgrading, don't return NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE so the caller
      // can pull out the transfer info for the new db.
      if (!newFile && summaryFileExists && !upgrading)
      {
        PRInt32 numNewMessages;
        PRUint32 folderSize;
        PRUint32  folderDate;
        nsFileSpec::TimeStamp actualFolderTimeStamp;
        
        mailDB->m_folderSpec->GetModDate(actualFolderTimeStamp) ;
        
        
        folderInfo->GetNumNewMessages(&numNewMessages);
        folderInfo->GetFolderSize(&folderSize);
        folderInfo->GetFolderDate(&folderDate);
        if (folderSize != mailDB->m_folderSpec->GetFileSize()||
          folderDate != actualFolderTimeStamp ||
          numNewMessages < 0)
          err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
      }
      // compare current version of db versus filed out version info.
      PRUint32 version;
      folderInfo->GetVersion(&version);
      if (mailDB->GetCurVersion() != version)
        err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
      NS_RELEASE(folderInfo);
    }
    if (err != NS_OK)
      deleteInvalidDB = PR_TRUE;
  }
  else
  {
    err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
    deleteInvalidDB = PR_TRUE;
  }

  if (deleteInvalidDB)
  {
    // this will make the db folder info release its ref to the mail db...
    NS_IF_RELEASE(mailDB->m_dbFolderInfo);
    mailDB->ForceClosed();
    NS_RELEASE(mailDB); // this sets mailDB to nsnull and makes ref count go to 0
    if (err == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
      summarySpec.Delete(PR_FALSE);
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
      NS_IF_RELEASE(mailDB);
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

// get this on demand so that only db's that have offline ops will
// create the table.	
nsresult nsMailDatabase::GetAllOfflineOpsTable()
{
  nsresult rv = NS_OK;
  if (!m_mdbAllOfflineOpsTable)
  {
		mdb_err err	= GetStore()->StringToToken(GetEnv(), kOfflineOpsScope, &m_offlineOpsRowScopeToken); 
    err = GetStore()->StringToToken(GetEnv(), kOfflineOpsTableKind, &m_offlineOpsTableKindToken); 
		gAllOfflineOpsTableOID.mOid_Scope = m_offlineOpsRowScopeToken;
		gAllOfflineOpsTableOID.mOid_Id = 1;

    rv = GetStore()->GetTable(GetEnv(), &gAllOfflineOpsTableOID, &m_mdbAllOfflineOpsTable);
    if (rv != NS_OK)
      rv = NS_ERROR_FAILURE;

    // create new all msg hdrs table, if it doesn't exist.
    if (NS_SUCCEEDED(rv) && !m_mdbAllOfflineOpsTable)
    {
	    nsIMdbStore *store = GetStore();
	    mdb_err mdberr = (nsresult) store->NewTable(GetEnv(), m_offlineOpsRowScopeToken, 
		    m_offlineOpsTableKindToken, PR_FALSE, nsnull, &m_mdbAllOfflineOpsTable);
      if (mdberr != NS_OK || !m_mdbAllOfflineOpsTable)
        rv = NS_ERROR_FAILURE;
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create offline ops table");
  }
  return rv;
}

/* static */ nsresult nsMailDatabase::CloneInvalidDBInfoIntoNewDB(nsFileSpec &pathName, nsMailDatabase** pMailDB)
{
	nsresult ret = NS_OK;
	return ret;
}

nsresult nsMailDatabase::OnNewPath (nsFileSpec &newPath)
{
	nsresult ret = NS_OK;
	return ret;
}

// cache m_folderStream to make updating mozilla status flags fast
NS_IMETHODIMP nsMailDatabase::StartBatch()
{
#ifndef XP_MAC
  if (!m_folderStream)
	  m_folderStream = new nsIOFileStream(nsFileSpec(*m_folderSpec));
#endif  
return NS_OK;
}

NS_IMETHODIMP nsMailDatabase::EndBatch()
{
#ifndef XP_MAC
  if (m_folderStream)
  {
    m_folderStream->close();  
    delete m_folderStream;
  }
  m_folderStream = nsnull;
#endif
  return NS_OK;
}


NS_IMETHODIMP nsMailDatabase::DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator)
{
	nsresult ret = NS_OK;
  if (!m_folderStream)
	  m_folderStream = new nsIOFileStream(nsFileSpec(*m_folderSpec));
	ret = nsMsgDatabase::DeleteMessages(nsMsgKeys, instigator);
	if (m_folderStream)
    {
      m_folderStream->close();
	  delete m_folderStream;
    }
	m_folderStream = NULL;
	SetFolderInfoValid(m_folderSpec, 0, 0);
	return ret;
}


// Helper routine - lowest level of flag setting
PRBool nsMailDatabase::SetHdrFlag(nsIMsgDBHdr *msgHdr, PRBool bSet, MsgFlags flag)
{
  nsIOFileStream *fileStream = NULL;
  PRBool		ret = PR_FALSE;
  
  if (nsMsgDatabase::SetHdrFlag(msgHdr, bSet, flag))
  {
    UpdateFolderFlag(msgHdr, bSet, flag, &fileStream);
    if (fileStream)
    {
      fileStream->close();
      delete fileStream;
      SetFolderInfoValid(m_folderSpec, 0, 0);
    }
    ret = PR_TRUE;
  }
  return ret;
}

#ifdef XP_MAC
extern PRFileDesc *gIncorporateFID;
extern const char* gIncorporatePath;
#endif // XP_MAC

// ### should move this into some utils class...
int msg_UnHex(char C)
{
	return ((C >= '0' && C <= '9') ? C - '0' :
			((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
			 ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)));
}


// We let the caller close the file in case he's updating a lot of flags
// and we don't want to open and close the file every time through.
// As an experiment, try caching the fid in the db as m_folderFile.
// If this is set, use it but don't return *pFid.
void nsMailDatabase::UpdateFolderFlag(nsIMsgDBHdr *mailHdr, PRBool bSet, 
							  MsgFlags flag, nsIOFileStream **ppFileStream)
{
  static char buf[50];
  nsIOFileStream *fileStream = (m_folderStream) ? m_folderStream : *ppFileStream;
  //#ifdef GET_FILE_STUFF_TOGETHER
#ifdef XP_MAC
  /* ducarroz: Do we still need this ??
  // This is a horrible hack and we should make sure we don't need it anymore.
  // It has to do with multiple people having the same file open, I believe, but the
  // mac file system only has one handle, and they compete for the file position.
  // Prevent closing the file from under the incorporate stuff. #82785.
  int32 savedPosition = -1;
  if (!fid && gIncorporatePath && !XP_STRCMP(m_folderSpec, gIncorporatePath))
  {
		fid = gIncorporateFID;
                savedPosition = ftell(gIncorporateFID); // so we can restore it.
                }
  */
#endif // XP_MAC
  PRUint32 offset;
  (void)mailHdr->GetStatusOffset(&offset);
  if (offset > 0) 
  {
    
    if (fileStream == NULL) 
    {
      fileStream = new nsIOFileStream(nsFileSpec(*m_folderSpec));
    }
    if (fileStream) 
    {
      PRUint32 msgOffset;
      (void)mailHdr->GetMessageOffset(&msgOffset);
      PRUint32 position = offset + msgOffset;
      PR_ASSERT(offset < 10000);
      fileStream->seek(position);
      buf[0] = '\0';
      if (fileStream->readline(buf, sizeof(buf))) 
      {
        if (strncmp(buf, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN) == 0 &&
          strncmp(buf + X_MOZILLA_STATUS_LEN, ": ", 2) == 0 &&
          strlen(buf) >= X_MOZILLA_STATUS_LEN + 6) 
        {
          PRUint32 flags;
          (void)mailHdr->GetFlags(&flags);
          if (!(flags & MSG_FLAG_EXPUNGED))
          {
            int i;
            char *p = buf + X_MOZILLA_STATUS_LEN + 2;
            
            for (i=0, flags = 0; i<4; i++, p++)
            {
              flags = (flags << 4) | msg_UnHex(*p);
            }
            
            PRUint32 curFlags;
            (void)mailHdr->GetFlags(&curFlags);
            flags = (flags & MSG_FLAG_QUEUED) |
              (curFlags & ~MSG_FLAG_RUNTIME_ONLY);
          }
          else
          {
            flags &= ~MSG_FLAG_RUNTIME_ONLY;
          }
          fileStream->seek(position);
          // We are filing out old Cheddar flags here
          PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS_FORMAT,
            flags & 0x0000FFFF);
          fileStream->write(buf, PL_strlen(buf));
          fileStream->flush();
          
          // time to upate x-mozilla-status2
          position = fileStream->tell();
          fileStream->seek(position + MSG_LINEBREAK_LEN);
          if (fileStream->readline(buf, sizeof(buf))) 
          {
            if (strncmp(buf, X_MOZILLA_STATUS2, X_MOZILLA_STATUS2_LEN) == 0 &&
              strncmp(buf + X_MOZILLA_STATUS2_LEN, ": ", 2) == 0 &&
              strlen(buf) >= X_MOZILLA_STATUS2_LEN + 10) 
            {
              PRUint32 dbFlags;
              (void)mailHdr->GetFlags(&dbFlags);
              dbFlags &= 0xFFFF0000;
              fileStream->seek(position + MSG_LINEBREAK_LEN);
              PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS2_FORMAT, dbFlags);
              fileStream->write(buf, PL_strlen(buf));
              fileStream->flush();
            }
          }
        } else 
        {
#ifdef DEBUG
          printf("Didn't find %s where expected at position %ld\n"
            "instead, found %s.\n",
            X_MOZILLA_STATUS, (long) position, buf);
#endif
          SetReparse(PR_TRUE);
        }			
      } 
      else 
      {
#ifdef DEBUG
        printf("Couldn't read old status line at all at position %ld\n",
          (long) position);
#endif
        SetReparse(PR_TRUE);
      }
#ifdef XP_MAC
      /* ducarroz: Do we still need this ??
      // Restore the file position
      if (savedPosition >= 0)
      XP_FileSeek(fid, savedPosition, SEEK_SET);
      */
#endif
    }
    else
    {
#ifdef DEBUG
      printf("Couldn't open mail folder for update%s!\n",
        (const char*)m_folderSpec);
#endif
      PR_ASSERT(PR_FALSE);
    }
  }
  //#endif // GET_FILE_STUFF_TOGETHER
#ifdef XP_MAC
  if (!m_folderStream /*&& fid != gIncorporateFID*/)	/* ducarroz: Do we still need this ?? */
#else
    if (!m_folderStream)
#endif
      *ppFileStream = fileStream; // This tells the caller that we opened the file, and please to close it.
}

/* static */  nsresult nsMailDatabase::SetSummaryValid(PRBool valid)
{
  nsresult ret = NS_OK;
  
  if (!m_folderSpec->Exists()) 
    return NS_MSG_ERROR_FOLDER_MISSING;
  
  if (m_dbFolderInfo)
  {
    if (valid)
    {
      nsFileSpec::TimeStamp actualFolderTimeStamp;
      m_folderSpec->GetModDate(actualFolderTimeStamp) ;
      
      m_dbFolderInfo->SetFolderSize(m_folderSpec->GetFileSize());
      m_dbFolderInfo->SetFolderDate(actualFolderTimeStamp);
    }
    else
    {
      m_dbFolderInfo->SetVersion(0);	// that ought to do the trick.
    }
  }
  return ret;
}

nsresult nsMailDatabase::GetFolderName(nsString &folderName)
{
	folderName.AssignWithConversion(NS_STATIC_CAST(const char*, *m_folderSpec));
	return NS_OK;
}


NS_IMETHODIMP  nsMailDatabase::RemoveOfflineOp(nsIMsgOfflineImapOperation *op)
{

  nsresult rv = GetAllOfflineOpsTable();
  NS_ENSURE_SUCCESS(rv, rv);

	if (!op || !m_mdbAllOfflineOpsTable)
		return NS_ERROR_NULL_POINTER;
  nsMsgOfflineImapOperation* offlineOp = NS_STATIC_CAST(nsMsgOfflineImapOperation*, op);  // closed system, so this is ok
	nsIMdbRow* row = offlineOp->GetMDBRow();
	nsresult ret = m_mdbAllOfflineOpsTable->CutRow(GetEnv(), row);
	row->CutAllColumns(GetEnv());
  return rv;
}

nsresult SetSourceMailbox(nsOfflineImapOperation *op, const char *mailbox, nsMsgKey key)
{
	nsresult ret = NS_OK;
	return ret;
}

	
nsresult nsMailDatabase::GetIdsWithNoBodies (nsMsgKeyArray &bodylessIds)
{
	nsresult ret = NS_OK;
	return ret;
}

NS_IMETHODIMP nsMailDatabase::GetOfflineOpForKey(nsMsgKey msgKey, PRBool create, nsIMsgOfflineImapOperation **offlineOp)
{
  PRBool newOp = PR_FALSE;
  mdb_bool	hasOid;
  mdbOid		rowObjectId;
  mdb_err   err;
  
  nsresult rv = GetAllOfflineOpsTable();
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!offlineOp || !m_mdbAllOfflineOpsTable)
    return NS_ERROR_NULL_POINTER;
  
  *offlineOp = NULL;
  
  rowObjectId.mOid_Id = msgKey;
  rowObjectId.mOid_Scope = m_offlineOpsRowScopeToken;
  err = m_mdbAllOfflineOpsTable->HasOid(GetEnv(), &rowObjectId, &hasOid);
  if (err == NS_OK && m_mdbStore && (hasOid  || create))
  {
    nsIMdbRow *offlineOpRow;
    err = m_mdbStore->GetRow(GetEnv(), &rowObjectId, &offlineOpRow);
    
    if (create)
    {
      if (!offlineOpRow)
      {
        err  = m_mdbStore->NewRowWithOid(GetEnv(), &rowObjectId, &offlineOpRow);
        NS_ENSURE_SUCCESS(err, err);
      }
      if (offlineOpRow && !hasOid)
      {
        m_mdbAllOfflineOpsTable->AddRow(GetEnv(), offlineOpRow);
        newOp = PR_TRUE;
      }
    }
    
    if (err == NS_OK && offlineOpRow)
    {
      *offlineOp = new nsMsgOfflineImapOperation(this, offlineOpRow);
      if (*offlineOp)
        (*offlineOp)->SetMessageKey(msgKey);
      nsCOMPtr <nsIMsgDBHdr> msgHdr;
      
      GetMsgHdrForKey(msgKey, getter_AddRefs(msgHdr));
      if (msgHdr)
      {
        imapMessageFlagsType imapFlags = kNoImapMsgFlag;
        PRUint32 msgHdrFlags;
        msgHdr->GetFlags(&msgHdrFlags);
        if (msgHdrFlags & MSG_FLAG_READ)
          imapFlags |= kImapMsgSeenFlag;
        if (msgHdrFlags & MSG_FLAG_REPLIED)
          imapFlags |= kImapMsgAnsweredFlag;
        if (msgHdrFlags & MSG_FLAG_MARKED)
          imapFlags |= kImapMsgFlaggedFlag;
        if (msgHdrFlags & MSG_FLAG_FORWARDED)
          imapFlags |= kImapMsgForwardedFlag;
        if (msgHdrFlags & MSG_FLAG_IMAP_DELETED)
          imapFlags |= kImapMsgDeletedFlag;
        (*offlineOp)->SetNewFlags(imapFlags);
      }
      NS_IF_ADDREF(*offlineOp);
    }
    if (!hasOid && m_dbFolderInfo)
    {
      PRInt32 newFlags;
      m_dbFolderInfo->OrFlags(MSG_FOLDER_FLAG_OFFLINEEVENTS, &newFlags);
    }
  }
  
  return (err == 0) ? NS_OK : NS_ERROR_FAILURE;

}

NS_IMETHODIMP nsMailDatabase::EnumerateOfflineOps(nsISimpleEnumerator **enumerator)
{
  NS_ASSERTION(PR_FALSE, "not impl yet");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMailDatabase::ListAllOfflineOpIds(nsMsgKeyArray *offlineOpIds)
{
  NS_ENSURE_ARG(offlineOpIds);
  nsresult rv = GetAllOfflineOpsTable();
  NS_ENSURE_SUCCESS(rv, rv);
	nsIMdbTableRowCursor *rowCursor;
	if (m_mdbAllOfflineOpsTable)
	{
		nsresult err = m_mdbAllOfflineOpsTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		while (err == NS_OK && rowCursor)
		{
			mdbOid outOid;
			mdb_pos	outPos;

			err = rowCursor->NextRowOid(GetEnv(), &outOid, &outPos);
			// is this right? Mork is returning a 0 id, but that should valid.
			if (outPos < 0 || outOid.mOid_Id == (mdb_id) -1)	
				break;
			if (err == NS_OK)
				offlineOpIds->Add(outOid.mOid_Id);
		}
    rv = (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
		rowCursor->Release();
	}

	offlineOpIds->QuickSort();
	return rv;
}

NS_IMETHODIMP nsMailDatabase::ListAllOfflineDeletes(nsMsgKeyArray *offlineDeletes)
{
	if (!offlineDeletes)
		return NS_ERROR_NULL_POINTER;

  nsresult rv = GetAllOfflineOpsTable();
  NS_ENSURE_SUCCESS(rv, rv);
	nsIMdbTableRowCursor *rowCursor;
	if (m_mdbAllOfflineOpsTable)
	{
		nsresult err = m_mdbAllOfflineOpsTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		while (err == NS_OK && rowCursor)
		{
			mdbOid outOid;
			mdb_pos	outPos;
      nsIMdbRow* offlineOpRow;

			err = rowCursor->NextRow(GetEnv(), &offlineOpRow, &outPos);
			// is this right? Mork is returning a 0 id, but that should valid.
			if (outPos < 0 || offlineOpRow == nsnull)	
				break;
			if (err == NS_OK)
      {
        offlineOpRow->GetOid(GetEnv(), &outOid);
        nsIMsgOfflineImapOperation *offlineOp = new nsMsgOfflineImapOperation(this, offlineOpRow);
        if (offlineOp)
        {
          NS_ADDREF(offlineOp);
          imapMessageFlagsType newFlags;
          nsOfflineImapOperationType opType;

          offlineOp->GetOperation(&opType);
          offlineOp->GetNewFlags(&newFlags);
          if (opType & nsIMsgOfflineImapOperation::kMsgMoved || 
              ((opType & nsIMsgOfflineImapOperation::kFlagsChanged) 
              && (newFlags & nsIMsgOfflineImapOperation::kMsgMarkedDeleted)))
    				offlineDeletes->Add(outOid.mOid_Id);
          NS_RELEASE(offlineOp);
        }
        offlineOpRow->Release();
      }
		}
    rv = (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
		rowCursor->Release();
	}
	return rv;
}

/* static */
nsresult nsMailDatabase::SetFolderInfoValid(nsFileSpec *folderName, int num, int numunread)
{
	nsLocalFolderSummarySpec	summarySpec(*folderName);
	nsFileSpec					summaryPath(summarySpec);
	nsresult		err = NS_OK;
	PRBool			bOpenedDB = PR_FALSE;

	if (!folderName->Exists())
		return NS_MSG_ERROR_FOLDER_SUMMARY_MISSING;

	// should we have type safe downcast methods again?
	nsMailDatabase *pMessageDB = (nsMailDatabase *) nsMailDatabase::FindInCache(summaryPath);
	if (pMessageDB == NULL)
	{
		pMessageDB = new nsMailDatabase();
		if(!pMessageDB)
			return NS_ERROR_OUT_OF_MEMORY;

		pMessageDB->m_folderSpec = new nsLocalFolderSummarySpec();
		if(!pMessageDB->m_folderSpec)
		{
			delete pMessageDB;
			return NS_ERROR_OUT_OF_MEMORY;
		}

		*(pMessageDB->m_folderSpec) = summarySpec;
		// ### this does later stuff (marks latered messages unread), which may be a problem
		err = pMessageDB->OpenMDB(summaryPath, PR_FALSE);
		if (err != NS_OK)
		{
			delete pMessageDB;
			pMessageDB = NULL;
		}
		bOpenedDB = PR_TRUE;
	}
	else
		pMessageDB->AddRef();


	if (pMessageDB == NULL)
	{
#ifdef DEBUG
		printf("Exception opening summary file\n");
#endif
		return NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
	}
	
	{
		nsFileSpec::TimeStamp actualFolderTimeStamp;
		folderName->GetModDate(actualFolderTimeStamp) ;

		pMessageDB->m_dbFolderInfo->SetFolderSize(folderName->GetFileSize());
		pMessageDB->m_dbFolderInfo->SetFolderDate(actualFolderTimeStamp);
		pMessageDB->m_dbFolderInfo->ChangeNumVisibleMessages(num);
		pMessageDB->m_dbFolderInfo->ChangeNumNewMessages(numunread);
		pMessageDB->m_dbFolderInfo->ChangeNumMessages(num);
	}
	// if we opened the db, then we'd better close it. Otherwise, we found it in the cache,
	// so just commit and release.
	if (bOpenedDB)
	{
		pMessageDB->Close(PR_TRUE);
	}
	else if (pMessageDB)
	{ 
		err = pMessageDB->Commit(nsMsgDBCommitType::kLargeCommit);
		pMessageDB->Release();
	}
	return err;
}


// This is used to remember that the db is out of sync with the mail folder
// and needs to be regenerated.
void nsMailDatabase::SetReparse(PRBool reparse)
{
	m_reparse = reparse;
}


static PRBool gGotThreadingPrefs = PR_FALSE;
static PRBool gThreadWithoutRe = PR_TRUE;


// should we thread messages with common subjects that don't start with Re: together?
// I imagine we might have separate preferences for mail and news, so this is a virtual method.
PRBool	nsMailDatabase::ThreadBySubjectWithoutRe()
{
	if (!gGotThreadingPrefs)
	{
		GetBoolPref("mail.thread_without_re", &gThreadWithoutRe);
		gGotThreadingPrefs = PR_TRUE;
	}

	gThreadWithoutRe = PR_TRUE;	// we need to this to be true for now.
	return gThreadWithoutRe;
}

class nsMsgOfflineOpEnumerator : public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS

  // nsISimpleEnumerator methods:
  NS_DECL_NSISIMPLEENUMERATOR

  nsMsgOfflineOpEnumerator(nsMailDatabase* db);
  virtual ~nsMsgOfflineOpEnumerator();

protected:
  nsresult					GetRowCursor();
  nsresult					PrefetchNext();
  nsMailDatabase*              mDB;
  nsIMdbTableRowCursor*       mRowCursor;
  nsCOMPtr <nsIMsgOfflineImapOperation> mResultOp;
  PRBool            mDone;
  PRBool						mNextPrefetched;
};

nsMsgOfflineOpEnumerator::nsMsgOfflineOpEnumerator(nsMailDatabase* db)
    : mDB(db), mRowCursor(nsnull), mDone(PR_FALSE)
{
  NS_INIT_REFCNT();
  NS_ADDREF(mDB);
  mNextPrefetched = PR_FALSE;
}

nsMsgOfflineOpEnumerator::~nsMsgOfflineOpEnumerator()
{
  if (mRowCursor)
    mRowCursor->CutStrongRef(mDB->GetEnv());
  NS_RELEASE(mDB);
}

NS_IMPL_ISUPPORTS1(nsMsgOfflineOpEnumerator, nsISimpleEnumerator)

nsresult nsMsgOfflineOpEnumerator::GetRowCursor()
{
  nsresult rv = 0;
  mDone = PR_FALSE;

  if (!mDB || !mDB->m_mdbAllOfflineOpsTable)
    return NS_ERROR_NULL_POINTER;

  rv = mDB->m_mdbAllOfflineOpsTable->GetTableRowCursor(mDB->GetEnv(), -1, &mRowCursor);
  return rv;
}

NS_IMETHODIMP nsMsgOfflineOpEnumerator::GetNext(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  nsresult rv=NS_OK;
  if (!mNextPrefetched)
    rv = PrefetchNext();
  if (NS_SUCCEEDED(rv))
  {
    if (mResultOp) 
    {
      *aItem = mResultOp;
      NS_ADDREF(*aItem);
      mNextPrefetched = PR_FALSE;
    }
  }
  return rv;
}

nsresult nsMsgOfflineOpEnumerator::PrefetchNext()
{
  nsresult rv = NS_OK;
  nsIMdbRow* offlineOpRow;
  mdb_pos rowPos;

  if (!mRowCursor)
  {
    rv = GetRowCursor();
    if (!NS_SUCCEEDED(rv))
      return rv;
  }

  rv = mRowCursor->NextRow(mDB->GetEnv(), &offlineOpRow, &rowPos);
  if (!offlineOpRow) 
  {
    mDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }
  if (NS_FAILED(rv)) 
  {
    mDone = PR_TRUE;
    return rv;
  }
	//Get key from row
  mdbOid outOid;
  nsMsgKey key=0;
  if (offlineOpRow->GetOid(mDB->GetEnv(), &outOid) == NS_OK)
    key = outOid.mOid_Id;

  nsIMsgOfflineImapOperation *op = new nsMsgOfflineImapOperation(mDB, offlineOpRow);
  mResultOp = op;
  if (!op)
    return NS_ERROR_OUT_OF_MEMORY;

  if (mResultOp) 
  {
    mNextPrefetched = PR_TRUE;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgOfflineOpEnumerator::HasMoreElements(PRBool *aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  if (!mNextPrefetched)
    PrefetchNext();
  *aResult = !mDone;
  return NS_OK;
}

