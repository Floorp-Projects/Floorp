/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "msgCore.h"    // precompiled header...
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIMsgFolder.h"	 
#include "nsIFileSpec.h"
#include "nsIStreamListener.h"
#include "nsIMsgMessageService.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsMsgUtils.h"
#include "nsIRDFService.h"
#include "nsIDBFolderInfo.h"
#include "nsIMessage.h"
#include "nsRDFCID.h"
#include "nsMsgFolderCompactor.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);

//////////////////////////////////////////////////////////////////////////////
// nsFolderCompactState
//////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS3(nsFolderCompactState, nsIMsgFolderCompactor, nsIStreamObserver, nsIStreamListener)

nsFolderCompactState::nsFolderCompactState()
{
  NS_INIT_ISUPPORTS();
  m_baseMessageUri = nsnull;
  m_fileStream = nsnull;
  m_size = 0;
  m_curIndex = -1;
  m_status = NS_OK;
  m_messageService = nsnull;
}

nsFolderCompactState::~nsFolderCompactState()
{
  if (m_fileStream)
  {
    m_fileStream->close();
    delete m_fileStream;
    m_fileStream = nsnull;
  }

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
    // if for some reason we failed remove the temp folder and database
    if (m_db)
      m_db->ForceClosed();
    nsLocalFolderSummarySpec summarySpec(m_fileSpec);
    m_fileSpec.Delete(PR_FALSE);
    summarySpec.Delete(PR_FALSE);
  }
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
  db ->ListAllKeys(m_keyArray);
  return rv;
}

nsresult
nsFolderCompactState::Init(nsIMsgFolder *folder, const char *baseMsgUri, nsIMsgDatabase *db,
                           nsIFileSpec *pathSpec)
{
  nsresult rv;

  m_folder = folder;
  m_baseMessageUri = nsCRT::strdup(baseMsgUri);
  if (!m_baseMessageUri)
    return NS_ERROR_OUT_OF_MEMORY;

  InitDB(db);

  m_size = m_keyArray.GetSize();
  m_curIndex = 0;
  pathSpec->GetFileSpec(&m_fileSpec);
  m_fileSpec.SetLeafName("nstmp");
  
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

NS_IMETHODIMP nsFolderCompactState::StartCompacting()
{
  nsresult rv = NS_OK;
  if (m_size > 0)
  {
    AddRef();
    rv = BuildMessageURI(m_baseMessageUri,
                                m_keyArray[0],
                                m_messageUri);
    if (NS_SUCCEEDED(rv))
      rv = m_messageService->CopyMessage(
        m_messageUri, this, PR_FALSE, nsnull,
        /* ### should get msg window! */ nsnull, nsnull);
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
  PRUint32 flags;

    // get leaf name and database name of the folder
  m_folder->GetFlags(&flags);
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

    // close down database of the original folder and remove the folder node
    // and all it's message node from the tree
  m_folder->ForceDBClosed();
  m_folder->GetParent(getter_AddRefs(parent));
  parentFolder = do_QueryInterface(parent, &rv);
  m_folder->SetParent(nsnull);
  parentFolder->PropagateDelete(m_folder, PR_FALSE);

    // remove the old folder and database
  fileSpec.Delete(PR_FALSE);
  summarySpec.Delete(PR_FALSE);
    // rename the copied folder and database to be the original folder and
    // database 
  m_fileSpec.Rename((const char*) idlName);
  newSummarySpec.Rename(dbName);

    // add the node back the tree
  nsCOMPtr<nsIMsgFolder> child;
  nsAutoString folderName; folderName.AssignWithConversion((const char*) idlName);
  rv = parentFolder->AddSubfolder(&folderName, getter_AddRefs(child));
  if (child) 
  {
    child->SetFlags(flags);
    nsCOMPtr<nsISupports> childSupports = do_QueryInterface(child);
    nsCOMPtr<nsISupports> parentSupports = do_QueryInterface(parentFolder);
    if (childSupports && parentSupports)
    {
      parentFolder->NotifyItemAdded(parentSupports, childSupports,
                                    "folderView");
    }
  }
  return rv;
}

nsresult
nsFolderCompactState::GetMessage(nsIMessage **message)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (!message) return rv;

	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFResource> msgResource;
		if(NS_SUCCEEDED(rdfService->GetResource(m_messageUri,
                                            getter_AddRefs(msgResource))))
			rv = msgResource->QueryInterface(NS_GET_IID(nsIMessage),
                                       (void**)message);
	}
  return rv;
}


NS_IMETHODIMP
nsFolderCompactState::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
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
nsFolderCompactState::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                    nsresult status, 
                                    const PRUnichar *errorMsg)
{
  nsresult rv = status;
  nsCOMPtr<nsIMessage> message;
  nsCOMPtr<nsIDBMessage> dbMessage;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;

  if (NS_FAILED(rv)) goto done;
  uri = do_QueryInterface(ctxt, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = GetMessage(getter_AddRefs(message));
  if (NS_FAILED(rv)) goto done;
  dbMessage = do_QueryInterface(message, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgHdr));
  if (NS_FAILED(rv)) goto done;

    // okay done with the current message; copying the existing message header
    // to the new database
  m_db->CopyHdrFromExistingHdr(m_startOfNewMsg, msgHdr, PR_TRUE,
                               getter_AddRefs(newMsgHdr));

  m_db->Commit(nsMsgDBCommitType::kLargeCommit);
    // advance to next message 
  m_curIndex ++;
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
    m_messageUri.SetLength(0); // clear the previous message uri
    rv = BuildMessageURI(m_baseMessageUri, m_keyArray[m_curIndex],
                                m_messageUri);
    if (NS_FAILED(rv)) goto done;
    rv = m_messageService->CopyMessage(m_messageUri, this, PR_FALSE, nsnull,
                                       /* ### should get msg window! */ nsnull, nsnull);
    
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

NS_IMETHODIMP
nsFolderCompactState::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
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
nsOfflineStoreCompactState::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                    nsresult status, 
                                    const PRUnichar *errorMsg)
{
  nsresult rv = status;
  nsCOMPtr<nsIMessage> message;
  nsCOMPtr<nsIDBMessage> dbMessage;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;

  if (NS_FAILED(rv)) goto done;
  uri = do_QueryInterface(ctxt, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = GetMessage(getter_AddRefs(message));
  if (NS_FAILED(rv)) goto done;
  dbMessage = do_QueryInterface(message, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgHdr));
  if (NS_FAILED(rv)) goto done;

  msgHdr->SetMessageOffset(m_startOfNewMsg);

  m_db->Commit(nsMsgDBCommitType::kLargeCommit);
    // advance to next message 
  m_curIndex ++;
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
  m_db->SetSummaryValid(PR_TRUE);
  m_db->Commit(nsMsgDBCommitType::kLargeCommit);

    // remove the old folder 
  fileSpec.Delete(PR_FALSE);

    // rename the copied folder to be the original folder 
  m_fileSpec.Rename((const char*) idlName);

  return rv;
}


