/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
#include "nsTextFormatter.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestorUtils.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
//////////////////////////////////////////////////////////////////////////////
// nsFolderCompactState
//////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS4(nsFolderCompactState, nsIMsgFolderCompactor, nsIRequestObserver, nsIStreamListener, nsICopyMessageStreamListener)

nsFolderCompactState::nsFolderCompactState()
{
  NS_INIT_ISUPPORTS();
  m_baseMessageUri = nsnull;
  m_fileStream = nsnull;
  m_size = 0;
  m_curIndex = -1;
  m_status = NS_OK;
  m_messageService = nsnull;
  m_compactAll = PR_FALSE;
  m_compactOfflineAlso = PR_FALSE;
  m_folderIndex =0;
}

nsFolderCompactState::~nsFolderCompactState()
{
  CloseOutputStream();
  if (m_messageService)
  {
    ReleaseMessageServiceFromURI(m_baseMessageUri, m_messageService);
    m_messageService = nsnull;
  }

  if (m_baseMessageUri)
  {
    nsCRT::free(m_baseMessageUri);
    m_baseMessageUri = nsnull;
  }

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

  db ->ListAllKeys(m_keyArray);
  nsresult rv = NS_NewFileSpecWithSpec(m_fileSpec, getter_AddRefs(newPathSpec));

  rv = nsComponentManager::CreateInstance(kCMailDB, nsnull,
                                          NS_GET_IID(nsIMsgDatabase),
                                          getter_AddRefs(mailDBFactory));
  if (NS_SUCCEEDED(rv)) 
  {
    nsresult folderOpen = mailDBFactory->Open(newPathSpec, PR_TRUE,
                                     PR_FALSE,
                                     getter_AddRefs(m_db));

    if(!NS_SUCCEEDED(folderOpen) &&
       folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || 
       folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
    {
      // if it's out of date then reopen with upgrade.
      rv = mailDBFactory->Open(newPathSpec,
                               PR_TRUE, PR_TRUE,
                               getter_AddRefs(m_db));
    }
  }
  return rv;
}

NS_IMETHODIMP nsFolderCompactState::StartCompactingAll(nsISupportsArray *aArrayOfFoldersToCompact, nsIMsgWindow *aMsgWindow, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray)
{
  nsresult rv = NS_OK;
  if (aArrayOfFoldersToCompact)  
    m_folderArray =do_QueryInterface(aArrayOfFoldersToCompact, &rv);

  if (NS_FAILED(rv) || !m_folderArray)
    return rv;
  
  m_window = aMsgWindow;
  m_compactAll = PR_TRUE;
  m_compactOfflineAlso = aCompactOfflineAlso;
  if (m_compactOfflineAlso)
    m_offlineFolderArray = do_QueryInterface(aOfflineFolderArray);

  m_folderIndex = 0;
  nsCOMPtr<nsISupports> supports = getter_AddRefs(m_folderArray->ElementAt(m_folderIndex));
  nsCOMPtr<nsIMsgFolder> firstFolder = do_QueryInterface(supports, &rv);

  if (NS_SUCCEEDED(rv) && firstFolder)
    CompactHelper(firstFolder);   //start with first folder from here.
  
  return rv;
}

nsresult 
nsFolderCompactState::CompactHelper(nsIMsgFolder *folder)
{
   nsresult rv = NS_ERROR_FAILURE;
   nsCOMPtr<nsIMsgDatabase> db;
   nsCOMPtr<nsIDBFolderInfo> folderInfo;
   nsCOMPtr<nsIMsgDatabase> mailDBFactory;
   nsCOMPtr<nsIFileSpec> pathSpec;
   char *baseMessageURI;

   rv = folder->GetMsgDatabase(nsnull, getter_AddRefs(db));
   NS_ENSURE_SUCCESS(rv,rv);

   rv = folder->GetPath(getter_AddRefs(pathSpec));
   NS_ENSURE_SUCCESS(rv,rv);

   rv = folder->GetBaseMessageURI(&baseMessageURI);
   NS_ENSURE_SUCCESS(rv,rv);
    
   rv = Init(folder, baseMessageURI, db, pathSpec, m_window);
   if (NS_SUCCEEDED(rv))
      rv = StartCompacting();

   if (baseMessageURI)
     nsCRT::free(baseMessageURI);
     
   return rv;
}



#define MESSENGER_STRING_URL       "chrome://messenger/locale/messenger.properties"

nsresult nsFolderCompactState::GetStringBundle(nsIStringBundle **aBundle)
{
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsresult res = NS_OK;

  nsCOMPtr<nsIStringBundleService> sBundleService = 
           do_GetService(kStringBundleServiceCID, &res);
  if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
  {
    res = sBundleService->CreateBundle(MESSENGER_STRING_URL, getter_AddRefs(stringBundle));
  }
  *aBundle = stringBundle;
  NS_IF_ADDREF(*aBundle);
  return res;
  }

nsresult nsFolderCompactState::GetStatusFromMsgName(const char *statusMsgName, PRUnichar ** retval)
  {
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsresult res = NS_OK;
  res = GetStringBundle(getter_AddRefs(stringBundle));
  if (stringBundle && NS_SUCCEEDED(res))
  {
		res = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2(statusMsgName).get(), retval);
  }
  return res;
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
  m_baseMessageUri = nsCRT::strdup(baseMsgUri);
  if (!m_baseMessageUri)
    return NS_ERROR_OUT_OF_MEMORY;

  pathSpec->GetFileSpec(&m_fileSpec);
  m_fileSpec.SetLeafName("nstmp");
  m_window = aMsgWindow;
  m_keyArray.RemoveAll();
  InitDB(db);

  m_size = m_keyArray.GetSize();
  m_curIndex = 0;
  
  m_fileStream = new nsOutputFileStream(m_fileSpec);
  if (!m_fileStream) 
  {
    rv = NS_ERROR_OUT_OF_MEMORY; 
  }
  else
  {
    rv = GetMessageServiceFromURI(baseMsgUri,
                                &m_messageService);
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
    nsXPIDLString formatString;

  nsresult rv = GetStatusFromMsgName("compactingFolder", getter_Copies(formatString));
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLString folderName;
      m_folder->GetName(getter_Copies(folderName));
      PRUnichar *u = nsTextFormatter::smprintf((const PRUnichar *) formatString, (const PRUnichar *) folderName);
      if (u)
        ShowStatusMsg(u);
      PR_FREEIF(u);
    }
}

void nsFolderCompactState::ThrowAlertMsg(const char *alertMsgName)
{
  nsresult rv=NS_OK;
  nsXPIDLString folderName;
  m_folder->GetName(getter_Copies(folderName));
  nsCOMPtr<nsIDocShell> docShell;
  if (m_window)
    m_window->GetRootDocShell(getter_AddRefs(docShell));
  if (docShell)
  {
    nsCOMPtr<nsIStringBundle> bundle;
    rv = GetStringBundle(getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv) && bundle)
    {
      const PRUnichar *formatStrings[] =
      {
        folderName
      };
      nsXPIDLString alertString;
      nsAutoString alertTemplate;
      alertTemplate.AssignWithConversion(alertMsgName);
      rv = bundle->FormatStringFromName(alertTemplate.get(),
                                    formatStrings, 1,
                                    getter_Copies(alertString));
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && alertString)
        dialog->Alert(nsnull, alertString);
    }
  }
}

NS_IMETHODIMP nsFolderCompactState::StartCompacting()
{
  nsresult rv = NS_OK;
  PRBool isLocked;
  nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMsgFolderCompactor*, this));
  m_folder->GetLocked(&isLocked);
  if(!isLocked)
    m_folder->AcquireSemaphore(supports);
  else
  {
    NS_ASSERTION(0, "Some other operation is in progress on this folder");
    m_folder->NotifyCompactCompleted();
    ThrowAlertMsg("compactFolderDeniedLock");
    if (m_compactAll)
      CompactNextFolder();
    else
    {
      CleanupTempFilesAfterError();
      return rv;
    }
  }
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
  nsCOMPtr<nsIFolder> parent;
  nsCOMPtr<nsIMsgFolder> parentFolder;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsFileSpec fileSpec;

    // get leaf name and database name of the folder
  rv = m_folder->GetPath(getter_AddRefs(pathSpec));
  pathSpec->GetFileSpec(&fileSpec);

  nsLocalFolderSummarySpec summarySpec(fileSpec);
  nsXPIDLCString idlName;
  nsString dbName;

  pathSpec->GetLeafName(getter_Copies(idlName));
  summarySpec.GetLeafName(dbName);

    // close down the temp file stream; preparing for deleting the old folder
    // and its database; then rename the temp folder and database
  m_fileStream->flush();
  m_fileStream->close();
  delete m_fileStream;
  m_fileStream = nsnull;

    // make sure the new database is valid
  m_db->SetSummaryValid(PR_TRUE);
  m_db->Commit(nsMsgDBCommitType::kLargeCommit);
  m_db->ForceClosed();
  m_db = null_nsCOMPtr();

  nsLocalFolderSummarySpec newSummarySpec(m_fileSpec);

  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  nsCOMPtr <nsIDBFolderInfo> transferInfo;
  nsCOMPtr <nsIMsgDatabase> db;
  m_folder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(db));
  if (dbFolderInfo)
    dbFolderInfo->GetTransferInfo(getter_AddRefs(transferInfo));
  db=nsnull;
    // close down database of the original folder and remove the folder node
    // and all it's message node from the tree
  m_folder->ForceDBClosed();
    // remove the old folder and database
  fileSpec.Delete(PR_FALSE);
  summarySpec.Delete(PR_FALSE);
    // rename the copied folder and database to be the original folder and
    // database 
  m_fileSpec.Rename((const char*) idlName);
  newSummarySpec.Rename(dbName);
 
  rv = ReleaseFolderLock();
  NS_ASSERTION(NS_SUCCEEDED(rv),"folder lock not released successfully");

  m_folder->GetMsgDatabase(m_window, getter_AddRefs(db));
  if (transferInfo && db)
  {
    dbFolderInfo=nsnull;
    db->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
    if (dbFolderInfo)
      dbFolderInfo->InitFromTransferInfo(transferInfo);
  }
  db = nsnull;
  dbFolderInfo=nsnull;
  transferInfo=nsnull;

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
       nsCOMPtr<nsISupports> supports = getter_AddRefs(m_folderArray->ElementAt(m_folderIndex-1));
       nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
       if (NS_SUCCEEDED(rv) && folder)
         folder->CompactAllOfflineStores(m_window, m_offlineFolderArray);
     }
     else
       return rv;
       
   } 
   nsCOMPtr<nsISupports> supports = getter_AddRefs(m_folderArray->ElementAt(m_folderIndex));
   nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);

   if (NS_SUCCEEDED(rv) && folder)
     rv = CompactHelper(folder);                    
   return rv;
}

nsresult
nsFolderCompactState::GetMessage(nsIMsgDBHdr **message)
{
  return GetMsgDBHdrFromURI(m_messageUri, message);
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
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;

  if (NS_FAILED(rv)) goto done;
  uri = do_QueryInterface(ctxt, &rv);
  if (NS_FAILED(rv)) goto done;
  EndCopy(nsnull, status);
  if (m_curIndex >= m_size)
  {
    msgHdr = nsnull;;
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
  nsresult rv = NS_ERROR_FAILURE;
  if (!m_fileStream || !inStr) return rv;

  PRUint32 maxReadCount, readCount, writeCount;
  rv = NS_OK;
  while (NS_SUCCEEDED(rv) && (PRInt32) count > 0)
  {
    maxReadCount = count > 4096 ? 4096 : count;
    rv = inStr->Read(m_dataBuffer, maxReadCount, &readCount);
    if (NS_SUCCEEDED(rv))
    {
      writeCount = m_fileStream->write(m_dataBuffer, readCount);
      count -= readCount;
      NS_ASSERTION (writeCount == readCount, 
                    "Oops, write fail, folder can be corrupted!\n");
      if (writeCount != readCount)
      {
        ThrowAlertMsg("compactFolderWriteFailed");
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
    msgHdr = nsnull;;
    newMsgHdr = nsnull;
    // no more to copy finish it up
   FinishCompact();
    Release(); // kill self
  }
  else
  {
    m_messageUri.SetLength(0); // clear the previous message uri
    rv = BuildMessageURI(m_baseMessageUri, m_keyArray[m_curIndex],
                                m_messageUri);
    if (NS_FAILED(rv)) goto done;
    rv = m_messageService->CopyMessage(m_messageUri, this, PR_FALSE, nsnull,
                                       /* ### should get msg window! */ nsnull, nsnull);
   if (!NS_SUCCEEDED(rv))
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

  nsXPIDLCString idlName;

  pathSpec->GetLeafName(getter_Copies(idlName));

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
  m_fileSpec.Rename((const char*) idlName);

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
    // record the new message key for the message
    m_fileStream->flush();
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
  m_messageUri.SetLength(0); // clear the previous message uri
  nsresult rv = BuildMessageURI(m_baseMessageUri, m_keyArray[m_curIndex],
                              m_messageUri);

  rv = GetMessage(getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);
    // okay done with the current message; copying the existing message header
    // to the new database
  m_db->CopyHdrFromExistingHdr(m_startOfNewMsg, msgHdr, PR_TRUE,
                               getter_AddRefs(newMsgHdr));

//  m_db->Commit(nsMsgDBCommitType::kLargeCommit);  // no sense commiting until the end
    // advance to next message 
  m_curIndex ++;
  nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;
  if (m_window)
  {
    m_window->GetStatusFeedback(getter_AddRefs(statusFeedback));
    if (statusFeedback)
      statusFeedback->ShowProgress (100 * m_curIndex / m_size);
  }
  return NS_OK;
}

NS_IMETHODIMP nsOfflineStoreCompactState::StartCompacting()
{
  nsresult rv = NS_OK;
  if (m_size > 0 && m_curIndex == 0)
  {
    AddRef(); // we own ourselves, until we're done, anyway.
    ShowCompactingStatusMsg();
    m_messageUri.SetLength(0); // clear the previous message uri
    rv = BuildMessageURI(m_baseMessageUri,
                                m_keyArray[0],
                                m_messageUri);
    if (NS_SUCCEEDED(rv))
      rv = m_messageService->CopyMessage(
        m_messageUri, this, PR_FALSE, nsnull, m_window, nsnull);

  }
  else
  { // no messages to copy with
    FinishCompact();
//    Release(); // we don't "own" ourselves yet.
  }
  return rv;
}

