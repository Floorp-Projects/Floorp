/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#include "msgCore.h"    // precompiled header...
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIMsgFolder.h"	 
#include "nsIMsgHdr.h"
#include "nsIFileSpec.h"
#include "nsIStreamListener.h"
#include "nsIMsgMessageService.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsMsgUtils.h"
#include "nsIRDFService.h"
#include "nsIDBFolderInfo.h"
#include "nsRDFCID.h"
#include "nsIDocShell.h"
#include "nsMsgFolderCompactor.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgImapMailFolder.h"

#include "nsMsgI18N.h"
#include "prprf.h"
#include "nsMsgLocalFolderHdrs.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
//////////////////////////////////////////////////////////////////////////////
// nsFolderCompactState
//////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS5(nsFolderCompactState, nsIMsgFolderCompactor, nsIRequestObserver, nsIStreamListener, nsICopyMessageStreamListener, nsIUrlListener)

nsFolderCompactState::nsFolderCompactState()
{
  m_fileStream = nsnull;
  m_size = 0;
  m_curIndex = -1;
  m_status = NS_OK;
  m_compactAll = PR_FALSE;
  m_compactOfflineAlso = PR_FALSE;
  m_compactingOfflineFolders = PR_FALSE;
  m_parsingFolder=PR_FALSE;
  m_folderIndex =0;
  m_startOfMsg = PR_TRUE;
  m_needStatusLine = PR_FALSE;
}

nsFolderCompactState::~nsFolderCompactState()
{
  CloseOutputStream();

  if (NS_FAILED(m_status))
  {
    CleanupTempFilesAfterError();
    // if for some reason we failed remove the temp folder and database
  }
}

void nsFolderCompactState::CloseOutputStream()
{
  if (m_fileStream)
  {
    m_fileStream->close();
    delete m_fileStream;
    m_fileStream = nsnull;
  }

}

void nsFolderCompactState::CleanupTempFilesAfterError()
{
  CloseOutputStream();
  if (m_db)
    m_db->ForceClosed();
  nsLocalFolderSummarySpec summarySpec(m_fileSpec);
  m_fileSpec.Delete(PR_FALSE);
  summarySpec.Delete(PR_FALSE);
}

nsresult nsFolderCompactState::BuildMessageURI(const char *baseURI, PRUint32 key, nsCString& uri)
{
	uri.Append(baseURI);
	uri.Append('#');
	uri.AppendInt(key);
	return NS_OK;
}


nsresult
nsFolderCompactState::InitDB(nsIMsgDatabase *db)
{
  nsCOMPtr<nsIMsgDatabase> mailDBFactory;
  nsCOMPtr<nsIFileSpec> newPathSpec;

  db->ListAllKeys(m_keyArray);
  nsresult rv = NS_NewFileSpecWithSpec(m_fileSpec, getter_AddRefs(newPathSpec));

  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  if (msgDBService) 
  {
    nsresult folderOpen = msgDBService->OpenMailDBFromFileSpec(newPathSpec, PR_TRUE,
                                     PR_FALSE,
                                     getter_AddRefs(m_db));

    if(NS_FAILED(folderOpen) &&
       folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || 
       folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
    {
      // if it's out of date then reopen with upgrade.
      rv = msgDBService->OpenMailDBFromFileSpec(newPathSpec,
                               PR_TRUE, PR_TRUE,
                               getter_AddRefs(m_db));
    }
  }
  return rv;
}

NS_IMETHODIMP nsFolderCompactState::CompactAll(nsISupportsArray *aArrayOfFoldersToCompact, nsIMsgWindow *aMsgWindow, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray)
{
  nsresult rv = NS_OK;
  m_window = aMsgWindow;
  if (aArrayOfFoldersToCompact)  
    m_folderArray =do_QueryInterface(aArrayOfFoldersToCompact, &rv);
  else if (aOfflineFolderArray)
  {
    m_folderArray = do_QueryInterface(aOfflineFolderArray, &rv);
    m_compactingOfflineFolders = PR_TRUE;
    aOfflineFolderArray = nsnull;
  }
  if (NS_FAILED(rv) || !m_folderArray)
    return rv;
 
  m_compactAll = PR_TRUE;
  m_compactOfflineAlso = aCompactOfflineAlso;
  if (m_compactOfflineAlso)
    m_offlineFolderArray = do_QueryInterface(aOfflineFolderArray);

  m_folderIndex = 0;
  nsCOMPtr<nsIMsgFolder> firstFolder = do_QueryElementAt(m_folderArray,
                                                         m_folderIndex, &rv);

  if (NS_SUCCEEDED(rv) && firstFolder)
    Compact(firstFolder, m_compactingOfflineFolders, aMsgWindow);   //start with first folder from here.
  
  return rv;
}

NS_IMETHODIMP
nsFolderCompactState::Compact(nsIMsgFolder *folder, PRBool aOfflineStore, nsIMsgWindow *aMsgWindow)
{
  if (!m_compactingOfflineFolders && !aOfflineStore)
{
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder);
    if (imapFolder)
      return folder->Compact(this, aMsgWindow);
  }
   m_window = aMsgWindow;
   nsresult rv;
   nsCOMPtr<nsIMsgDatabase> db;
   nsCOMPtr<nsIDBFolderInfo> folderInfo;
   nsCOMPtr<nsIMsgDatabase> mailDBFactory;
   nsCOMPtr<nsIFileSpec> pathSpec;
   nsXPIDLCString baseMessageURI;

   nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(folder, &rv);
   if (NS_SUCCEEDED(rv) && localFolder)
   {
     rv=localFolder->GetDatabaseWOReparse(getter_AddRefs(db));
     if (NS_FAILED(rv) || !db)
     {
       if (rv == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || rv == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
       {
         m_folder =folder;  //will be used to compact
         m_parsingFolder = PR_TRUE;
         rv = localFolder->ParseFolder(m_window, this);
       }
       return rv;
     }
     else
     {
       PRBool valid;  
       rv = db->GetSummaryValid(&valid); 
       if (!valid) //we are probably parsing the folder because we selected it.
       {
         folder->NotifyCompactCompleted();
         if (m_compactAll)
           return CompactNextFolder();
         else
           return NS_OK;
       }
     }
   }
   else
   {
     rv=folder->GetMsgDatabase(nsnull, getter_AddRefs(db));
     NS_ENSURE_SUCCESS(rv,rv);
   }
   rv = folder->GetPath(getter_AddRefs(pathSpec));
   NS_ENSURE_SUCCESS(rv,rv);

   rv = folder->GetBaseMessageURI(getter_Copies(baseMessageURI));
   NS_ENSURE_SUCCESS(rv,rv);
    
   rv = Init(folder, baseMessageURI, db, pathSpec, m_window);
   NS_ENSURE_SUCCESS(rv,rv);

   PRBool isLocked;
   m_folder->GetLocked(&isLocked);
   if(!isLocked)
   {
     nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMsgFolderCompactor*, this));
     m_folder->AcquireSemaphore(supports);
     return StartCompacting();
   }
   else
   {
     m_folder->NotifyCompactCompleted();
     m_folder->ThrowAlertMsg("compactFolderDeniedLock", m_window);
     CleanupTempFilesAfterError();
     if (m_compactAll)
       return CompactNextFolder();
     else
       return NS_OK;
   }
}

nsresult nsFolderCompactState::ShowStatusMsg(const PRUnichar *aMsg)
{
  nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;
  if (m_window)
  {
    m_window->GetStatusFeedback(getter_AddRefs(statusFeedback));
    if (statusFeedback && aMsg)
      return statusFeedback->ShowStatusString (aMsg);
  }
  return NS_OK;
}

nsresult
nsFolderCompactState::Init(nsIMsgFolder *folder, const char *baseMsgUri, nsIMsgDatabase *db,
                           nsIFileSpec *pathSpec, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;

  m_folder = folder;
  m_baseMessageUri = baseMsgUri;

  pathSpec->GetFileSpec(&m_fileSpec);

  // need to make sure the temp file goes in the same real directory
  // as the original file, so resolve sym links.
  PRBool ignored;
  m_fileSpec.ResolveSymlink(ignored);

  m_fileSpec.SetLeafName("nstmp");
  m_fileSpec.MakeUnique();   //make sure we are not crunching existing nstmp file
  m_window = aMsgWindow;
  m_keyArray.RemoveAll();
  InitDB(db);

  m_size = m_keyArray.GetSize();
  m_curIndex = 0;
  
  m_fileStream = new nsOutputFileStream(m_fileSpec);
  if (!m_fileStream) 
  {
    m_folder->ThrowAlertMsg("compactFolderWriteFailed", m_window);
    rv = NS_ERROR_OUT_OF_MEMORY; 
  }
  else
  {
    rv = GetMessageServiceFromURI(baseMsgUri,
                                getter_AddRefs(m_messageService));
  }
  if (NS_FAILED(rv))
  {
    m_status = rv;
    Release(); // let go of ourselves...
  }
  return rv;
}

void nsFolderCompactState::ShowCompactingStatusMsg()
{
  nsXPIDLString statusString;
  nsresult rv = m_folder->GetStringWithFolderNameFromBundle("compactingFolder", getter_Copies(statusString));
  if (statusString && NS_SUCCEEDED(rv))
    ShowStatusMsg(statusString);
}

NS_IMETHODIMP nsFolderCompactState::OnStartRunningUrl(nsIURI *url)
{
  return NS_OK;
}

NS_IMETHODIMP nsFolderCompactState::OnStopRunningUrl(nsIURI *url, nsresult status)
{
  if (m_parsingFolder)
  {
    m_parsingFolder=PR_FALSE;
    if (NS_SUCCEEDED(status))
      status=Compact(m_folder, m_compactingOfflineFolders, m_window);
    else if (m_compactAll)
      CompactNextFolder();
  }
  else if (m_compactAll) // this should be the imap case only
    CompactNextFolder();
  return NS_OK;
}

nsresult nsFolderCompactState::StartCompacting()
{
  nsresult rv = NS_OK;
  if (m_size > 0)
  {
    ShowCompactingStatusMsg();
    AddRef();
    rv = m_messageService->CopyMessages(&m_keyArray, m_folder, this, PR_FALSE, nsnull, m_window, nsnull);
    // m_curIndex = m_size;  // advance m_curIndex to the end - we're done

  }
  else
  { // no messages to copy with
    FinishCompact();
//    Release(); // we don't "own" ourselves yet.
  }
  return rv;
}

nsresult
nsFolderCompactState::FinishCompact()
{
    // All okay time to finish up the compact process
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileSpec> pathSpec;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsFileSpec fileSpec;

    // get leaf name and database name of the folder
  rv = m_folder->GetPath(getter_AddRefs(pathSpec));
  pathSpec->GetFileSpec(&fileSpec);

  // need to make sure we put the .msf file in the same directory
  // as the original mailbox, so resolve symlinks.
  PRBool ignored;
  fileSpec.ResolveSymlink(ignored);

  nsLocalFolderSummarySpec summarySpec(fileSpec);
  nsXPIDLCString leafName;
  nsCAutoString dbName(summarySpec.GetLeafName());

  pathSpec->GetLeafName(getter_Copies(leafName));

    // close down the temp file stream; preparing for deleting the old folder
    // and its database; then rename the temp folder and database
  m_fileStream->flush();
  m_fileStream->close();
  delete m_fileStream;
  m_fileStream = nsnull;

  // make sure the new database is valid.
  // Close it so we can rename the .msf file.
  m_db->SetSummaryValid(PR_TRUE);
  m_db->Commit(nsMsgDBCommitType::kLargeCommit);
  m_db->ForceClosed();
  m_db = nsnull;

  nsLocalFolderSummarySpec newSummarySpec(m_fileSpec);

  nsCOMPtr <nsIDBFolderInfo> transferInfo;
  m_folder->GetDBTransferInfo(getter_AddRefs(transferInfo));

  // close down database of the original folder and remove the folder node
  // and all it's message node from the tree
  m_folder->ForceDBClosed();
    // remove the old folder and database
  fileSpec.Delete(PR_FALSE);
  summarySpec.Delete(PR_FALSE);
    // rename the copied folder and database to be the original folder and
    // database 
  m_fileSpec.Rename(leafName.get());
  newSummarySpec.Rename(dbName.get());
 
  rv = ReleaseFolderLock();
  NS_ASSERTION(NS_SUCCEEDED(rv),"folder lock not released successfully");
  m_folder->SetDBTransferInfo(transferInfo);

  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;

  m_folder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(m_db));

  // since we're transferring info from the old db, we need to reset the expunged bytes,
  // and set the summary valid again.
  if(dbFolderInfo)
    dbFolderInfo->SetExpungedBytes(0);
  m_db->Close(PR_TRUE);
  m_db = nsnull;

  m_folder->NotifyCompactCompleted();

  if (m_compactAll)
    rv = CompactNextFolder();
      
  return rv;
}

nsresult
nsFolderCompactState::ReleaseFolderLock()
{
  nsresult result = NS_OK;
  if (!m_folder) return result;
  PRBool haveSemaphore;
  nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMsgFolderCompactor*, this));
  result = m_folder->TestSemaphore(supports, &haveSemaphore);
  if(NS_SUCCEEDED(result) && haveSemaphore)
    result = m_folder->ReleaseSemaphore(supports);
  return result;
}

nsresult
nsFolderCompactState::CompactNextFolder()
{
   nsresult rv = NS_OK;
   m_folderIndex++;
   PRUint32 cnt=0;
   rv = m_folderArray->Count(&cnt);
   NS_ENSURE_SUCCESS(rv,rv);
   if (m_folderIndex == cnt)
   {
     if (m_compactOfflineAlso)
     {
       m_compactingOfflineFolders = PR_TRUE;
       nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(m_folderArray,
                                                         m_folderIndex-1, &rv);
       if (NS_SUCCEEDED(rv) && folder)
         folder->CompactAllOfflineStores(m_window, m_offlineFolderArray);
     }
     else
       return rv;
       
   } 
   nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(m_folderArray,
                                                     m_folderIndex, &rv);

   if (NS_SUCCEEDED(rv) && folder)
     rv = Compact(folder, m_compactingOfflineFolders, m_window);                    
   return rv;
}

nsresult
nsFolderCompactState::GetMessage(nsIMsgDBHdr **message)
{
  return GetMsgDBHdrFromURI(m_messageUri.get(), message);
}


NS_IMETHODIMP
nsFolderCompactState::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return StartMessage();
}

NS_IMETHODIMP
nsFolderCompactState::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                    nsresult status)
{
  nsresult rv = status;
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;

  if (NS_FAILED(rv)) goto done;
  EndCopy(nsnull, status);
  if (m_curIndex >= m_size)
  {
    msgHdr = nsnull;
    newMsgHdr = nsnull;
    // no more to copy finish it up
   FinishCompact();
    Release(); // kill self
  }
  else
  {
    // in case we're not getting an error, we still need to pretend we did get an error,
    // because the compact did not successfully complete.
    if (NS_SUCCEEDED(status))
    {
      m_folder->NotifyCompactCompleted();
      CleanupTempFilesAfterError();
      ReleaseFolderLock();
      Release();
    }
  }

done:
  if (NS_FAILED(rv)) {
    m_status = rv; // set the status to rv so the destructor can remove the
                   // temp folder and database
    m_folder->NotifyCompactCompleted();
    ReleaseFolderLock();
    Release(); // kill self
    return rv;
  }
  return rv;
}

NS_IMETHODIMP
nsFolderCompactState::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                                      nsIInputStream *inStr, 
                                      PRUint32 sourceOffset, PRUint32 count)
{
  if (!m_fileStream || !inStr) 
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  PRUint32 msgFlags;
  if (m_startOfMsg)
  {
    m_statusOffset = 0;
    m_messageUri.SetLength(0); // clear the previous message uri
    if (NS_SUCCEEDED(BuildMessageURI(m_baseMessageUri.get(), m_keyArray[m_curIndex],
                                m_messageUri)))
    {
      rv = GetMessage(getter_AddRefs(m_curSrcHdr));
      NS_ENSURE_SUCCESS(rv, rv);
      if (m_curSrcHdr)
      {
        (void) m_curSrcHdr->GetFlags(&msgFlags);
        PRUint32 statusOffset;
        (void) m_curSrcHdr->GetStatusOffset(&statusOffset);
        if (statusOffset == 0)
          m_needStatusLine = PR_TRUE;
      }
    }
    m_startOfMsg = PR_FALSE;
  }
  PRUint32 maxReadCount, readCount, writeCount;
  while (NS_SUCCEEDED(rv) && (PRInt32) count > 0)
  {
    maxReadCount = count > 4096 ? 4096 : count;
    rv = inStr->Read(m_dataBuffer, maxReadCount, &readCount);
    if (NS_SUCCEEDED(rv))
    {
      if (m_needStatusLine)
      {
        m_needStatusLine = PR_FALSE;
        // we need to parse out the "From " header, write it out, then 
        // write out the x-mozilla-status headers, and set the 
        // status offset of the dest hdr for later use 
        // in OnEndCopy).
        if (!strncmp(m_dataBuffer, "From ", 5))
        {
          PRInt32 charIndex;
          for (charIndex = 5; charIndex < readCount; charIndex++)
          {
            if (m_dataBuffer[charIndex] == nsCRT::CR || m_dataBuffer[charIndex] == nsCRT::LF)
            {
              charIndex++;
              if (m_dataBuffer[charIndex- 1] == nsCRT::CR && m_dataBuffer[charIndex] == nsCRT::LF)
                charIndex++;
              break;
            }
          }
          char statusLine[50];
          writeCount = m_fileStream->write(m_dataBuffer, charIndex);
          m_statusOffset = charIndex;
          PR_snprintf(statusLine, sizeof(statusLine), X_MOZILLA_STATUS_FORMAT MSG_LINEBREAK, msgFlags & 0xFFFF);
          m_statusLineSize = m_fileStream->write(statusLine, strlen(statusLine));
          PR_snprintf(statusLine, sizeof(statusLine), X_MOZILLA_STATUS2_FORMAT MSG_LINEBREAK, msgFlags & 0xFFFF0000);
          m_statusLineSize += m_fileStream->write(statusLine, strlen(statusLine));
          writeCount += m_fileStream->write(m_dataBuffer + charIndex, readCount - charIndex);

        }
        else
        {
          NS_ASSERTION(PR_FALSE, "not an envelope");
          // try to mark the db as invalid so it will be reparsed.
          nsCOMPtr <nsIMsgDatabase> srcDB;
          m_folder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
          if (srcDB)
          {
            srcDB->SetSummaryValid(PR_FALSE);
            srcDB->ForceClosed();
          }
        }
      }
      else
      {
        writeCount = m_fileStream->write(m_dataBuffer, readCount);
      }
      count -= readCount;
      if (writeCount != readCount)
      {
        m_folder->ThrowAlertMsg("compactFolderWriteFailed", m_window);
        return NS_MSG_ERROR_WRITING_MAIL_FOLDER;
      }
    }
  }
  return rv;
}

nsOfflineStoreCompactState::nsOfflineStoreCompactState()
{
}

nsOfflineStoreCompactState::~nsOfflineStoreCompactState()
{
}


nsresult
nsOfflineStoreCompactState::InitDB(nsIMsgDatabase *db)
{
  db->ListAllOfflineMsgs(&m_keyArray);
  m_db = db;
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineStoreCompactState::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                          nsresult status)
{
  nsresult rv = status;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;
  nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;
  ReleaseFolderLock();

  if (NS_FAILED(rv)) goto done;
  uri = do_QueryInterface(ctxt, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = GetMessage(getter_AddRefs(msgHdr));
  if (NS_FAILED(rv)) goto done;

  if (msgHdr)
    msgHdr->SetMessageOffset(m_startOfNewMsg);

  if (m_window)
  {
    m_window->GetStatusFeedback(getter_AddRefs(statusFeedback));
    if (statusFeedback)
      statusFeedback->ShowProgress (100 * m_curIndex / m_size);
  }
    // advance to next message 
  m_curIndex ++;
  if (m_curIndex >= m_size)
  {
    m_db->Commit(nsMsgDBCommitType::kLargeCommit);
    msgHdr = nsnull;
    newMsgHdr = nsnull;
    // no more to copy finish it up
   FinishCompact();
    Release(); // kill self
  }
  else
  {
    m_messageUri.SetLength(0); // clear the previous message uri
    rv = BuildMessageURI(m_baseMessageUri.get(), m_keyArray[m_curIndex],
                                m_messageUri);
    if (NS_FAILED(rv)) goto done;
    rv = m_messageService->CopyMessage(m_messageUri.get(), this, PR_FALSE, nsnull,
                                       /* ### should get msg window! */ nsnull, nsnull);
   if (NS_FAILED(rv))
   {
     PRUint32 resultFlags;
     msgHdr->AndFlags(~MSG_FLAG_OFFLINE, &resultFlags);
   }
  // if this fails, we should clear the offline flag on the source message.
    
  }

done:
  if (NS_FAILED(rv)) {
    m_status = rv; // set the status to rv so the destructor can remove the
                   // temp folder and database
    Release(); // kill self
    return rv;
  }
  return rv;
}
 

nsresult
nsOfflineStoreCompactState::FinishCompact()
{
    // All okay time to finish up the compact process
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileSpec> pathSpec;
  nsFileSpec fileSpec;
  PRUint32 flags;

    // get leaf name and database name of the folder
  m_folder->GetFlags(&flags);
  rv = m_folder->GetPath(getter_AddRefs(pathSpec));
  pathSpec->GetFileSpec(&fileSpec);

  nsXPIDLCString leafName;

  pathSpec->GetLeafName(getter_Copies(leafName));

    // close down the temp file stream; preparing for deleting the old folder
    // and its database; then rename the temp folder and database
  m_fileStream->flush();
  m_fileStream->close();
  delete m_fileStream;
  m_fileStream = nsnull;

    // make sure the new database is valid
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  m_db->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
  if (dbFolderInfo)
    dbFolderInfo->SetExpungedBytes(0);
  m_folder->UpdateSummaryTotals(PR_TRUE);
  m_db->SetSummaryValid(PR_TRUE);
  m_db->Commit(nsMsgDBCommitType::kLargeCommit);

    // remove the old folder 
  fileSpec.Delete(PR_FALSE);

    // rename the copied folder to be the original folder 
  m_fileSpec.Rename(leafName.get());

  PRUnichar emptyStr = 0;
  ShowStatusMsg(&emptyStr);
  if (m_compactAll)
    rv = CompactNextFolder();
  return rv;
}


NS_IMETHODIMP
nsFolderCompactState::Init(nsIMsgFolder *srcFolder, nsICopyMessageListener *destination, nsISupports *listenerData)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFolderCompactState::StartMessage()
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ASSERTION(m_fileStream, "Fatal, null m_fileStream...\n");
  if (m_fileStream)
  {
    // this will force an internal flush, but not a sync. Tell should really do an internal flush,
    // but it doesn't, and I'm afraid to change that nsIFileStream.cpp code anymore.
    m_fileStream->seek(PR_SEEK_CUR, 0);
    // record the new message key for the message
    m_startOfNewMsg = m_fileStream->tell();
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsFolderCompactState::EndMessage(nsMsgKey key)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFolderCompactState::EndCopy(nsISupports *url, nsresult aStatus)
{
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;

  if (m_curIndex >= m_size)
  {
    NS_ASSERTION(PR_FALSE, "m_curIndex out of bounds");
    return NS_OK;
  }

    // okay done with the current message; copying the existing message header
    // to the new database
  if (m_curSrcHdr)
    m_db->CopyHdrFromExistingHdr(m_startOfNewMsg, m_curSrcHdr, PR_TRUE,
                               getter_AddRefs(newMsgHdr));
  m_curSrcHdr = nsnull;
  if (newMsgHdr && m_statusOffset != 0)
  {
    PRUint32 oldMsgSize;
    newMsgHdr->SetStatusOffset(m_statusOffset);
    (void) newMsgHdr->GetMessageSize(&oldMsgSize);
    newMsgHdr->SetMessageSize(oldMsgSize + m_statusLineSize);
  }

//  m_db->Commit(nsMsgDBCommitType::kLargeCommit);  // no sense commiting until the end
    // advance to next message 
  m_curIndex ++;
  m_startOfMsg = PR_TRUE;
  nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;
  if (m_window)
  {
    m_window->GetStatusFeedback(getter_AddRefs(statusFeedback));
    if (statusFeedback)
      statusFeedback->ShowProgress (100 * m_curIndex / m_size);
  }
  return NS_OK;
}

nsresult nsOfflineStoreCompactState::StartCompacting()
{
  nsresult rv = NS_OK;
  if (m_size > 0 && m_curIndex == 0)
  {
    AddRef(); // we own ourselves, until we're done, anyway.
    ShowCompactingStatusMsg();
    m_messageUri.SetLength(0); // clear the previous message uri
    rv = BuildMessageURI(m_baseMessageUri.get(),
                                m_keyArray[0],
                                m_messageUri);
    if (NS_SUCCEEDED(rv))
      rv = m_messageService->CopyMessage(
        m_messageUri.get(), this, PR_FALSE, nsnull, m_window, nsnull);

  }
  else
  { // no messages to copy with
    ReleaseFolderLock();
    FinishCompact();
//    Release(); // we don't "own" ourselves yet.
  }
  return rv;
}

