/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsImapOfflineSync.h"
#include "nsImapMailFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsXPIDLString.h"
#include "nsIRDFService.h"
#include "nsMsgBaseCID.h"
#include "nsRDFCID.h"
#include "nsIMsgAccountManager.h"
#include "nsINntpIncomingServer.h"
#include "nsIRequestObserver.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIFileStream.h"
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

NS_IMPL_ISUPPORTS2(nsImapOfflineSync, nsIUrlListener, nsIMsgCopyServiceListener)

nsImapOfflineSync::nsImapOfflineSync(nsIMsgWindow *window, nsIUrlListener *listener, nsIMsgFolder *singleFolderOnly)
{
  NS_INIT_REFCNT();
  m_singleFolderToUpdate = singleFolderOnly;
  m_window = window;
  // not the perfect place for this, but I think it will work.
  if (m_window)
    m_window->SetStopped(PR_FALSE);

  mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kFlagsChanged;
  m_mailboxupdatesStarted = PR_FALSE;
  m_mailboxupdatesFinished = PR_FALSE;
  m_createdOfflineFolders = PR_FALSE;
  m_pseudoOffline = PR_FALSE;
  m_KeyIndex = 0;
  mCurrentUIDValidity = nsMsgKey_None;
  m_listener = listener;
}

nsImapOfflineSync::~nsImapOfflineSync()
{
}

void      nsImapOfflineSync::SetWindow(nsIMsgWindow *window)
{
  m_window = window;
}

NS_IMETHODIMP nsImapOfflineSync::OnStartRunningUrl(nsIURI* url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsImapOfflineSync::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  nsresult rv = exitCode;

  // where do we make sure this gets cleared when we start running urls?
  PRBool stopped = PR_FALSE;
  if (m_window)
    m_window->GetStopped(&stopped);
  if (stopped)
    exitCode = NS_BINDING_ABORTED;

  if (m_curTempFile)
  {
    m_curTempFile->Delete(PR_FALSE);
    m_curTempFile = nsnull;
  }
  if (NS_SUCCEEDED(exitCode))
    rv = ProcessNextOperation();
  else if (m_listener)  // notify main observer.
    m_listener->OnStopRunningUrl(url, exitCode);

  return rv;
}

// leaves m_currentServer at the next imap or local mail "server" that
// might have offline events to playback. If no more servers,
// m_currentServer will be left at nsnull.
// Also, sets up m_serverEnumerator to enumerate over the server
nsresult nsImapOfflineSync::AdvanceToNextServer()
{
  nsresult rv;

  if (!m_allServers)
  {
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ASSERTION(accountManager && NS_SUCCEEDED(rv), "couldn't get account mgr");
    if (!accountManager || NS_FAILED(rv)) return rv;

    rv = accountManager->GetAllServers(getter_AddRefs(m_allServers));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  PRUint32 serverIndex = (m_currentServer) ? m_allServers->IndexOf(m_currentServer) + 1 : 0;
  m_currentServer = nsnull;
  PRUint32 numServers; 
  m_allServers->Count(&numServers);
  nsCOMPtr <nsIFolder> rootFolder;

  while (serverIndex < numServers)
  {
    nsCOMPtr <nsISupports> serverSupports = getter_AddRefs(m_allServers->ElementAt(serverIndex));
    serverIndex++;

    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverSupports);
    nsCOMPtr <nsINntpIncomingServer> newsServer = do_QueryInterface(server);
    if (newsServer) // news servers aren't involved in offline imap
      continue;
    if (server)
    {
      m_currentServer = server;
      server->GetRootFolder(getter_AddRefs(rootFolder));
      if (rootFolder)
      {
        NS_NewISupportsArray(getter_AddRefs(m_allFolders));
        rv = rootFolder->ListDescendents(m_allFolders);
        if (NS_SUCCEEDED(rv))
          m_allFolders->Enumerate(getter_AddRefs(m_serverEnumerator));
        if (NS_SUCCEEDED(rv) && m_serverEnumerator)
        {
          rv = m_serverEnumerator->First();
          if (NS_SUCCEEDED(rv))
            break;
        }
      }
    }
  }
  return rv;
}

nsresult nsImapOfflineSync::AdvanceToNextFolder()
{
  nsresult rv;
	// we always start by changing flags
  mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kFlagsChanged;
	
  m_currentFolder = nsnull;

  if (!m_currentServer)
     rv = AdvanceToNextServer();
  else
    rv = m_serverEnumerator->Next();
  if (!NS_SUCCEEDED(rv))
    rv = AdvanceToNextServer();

  if (NS_SUCCEEDED(rv) && m_serverEnumerator)
  {
    // ### argh, this doesn't go into sub-folders of each folder.
    nsCOMPtr <nsISupports> supports;
    rv = m_serverEnumerator->CurrentItem(getter_AddRefs(supports));
    m_currentFolder = do_QueryInterface(supports);
  }
  return rv;
}

void nsImapOfflineSync::AdvanceToFirstIMAPFolder()
{
  nsresult rv;
  m_currentServer = nsnull;
  nsCOMPtr <nsIMsgImapMailFolder> imapFolder;
  do
  {
    rv = AdvanceToNextFolder();
    if (m_currentFolder)
      imapFolder = do_QueryInterface(m_currentFolder);
  }
  while (NS_SUCCEEDED(rv) && m_currentFolder && !imapFolder);
}

void nsImapOfflineSync::ProcessFlagOperation(nsIMsgOfflineImapOperation *currentOp)
{
	nsMsgKeyArray matchingFlagKeys;
	PRUint32 currentKeyIndex = m_KeyIndex;
	imapMessageFlagsType matchingFlags;
  currentOp->GetNewFlags(&matchingFlags);
  imapMessageFlagsType flagOperation;
  imapMessageFlagsType newFlags;
	
	do
  {	// loop for all messsages with the same flags
    nsMsgKey curKey;
    currentOp->GetMessageKey(&curKey);
		matchingFlagKeys.Add(curKey);
    currentOp->ClearOperation(nsIMsgOfflineImapOperation::kFlagsChanged);
    currentOp = nsnull;
    if (++currentKeyIndex < m_CurrentKeys.GetSize())
      m_currentDB->GetOfflineOpForKey(m_CurrentKeys[currentKeyIndex], PR_FALSE,
        &currentOp);
    if (currentOp)
    {
      currentOp->GetFlagOperation(&flagOperation);
      currentOp->GetNewFlags(&newFlags);
    }
	} while (currentOp && (flagOperation & nsIMsgOfflineImapOperation::kFlagsChanged) && (newFlags == matchingFlags) );
	
  currentOp = nsnull;
	
  if (matchingFlagKeys.GetSize() > 0)
  {
    nsCAutoString uids;
	  nsImapMailFolder::AllocateUidStringFromKeys(matchingFlagKeys.GetArray(), matchingFlagKeys.GetSize(), uids);
    PRUint32 curFolderFlags;
    m_currentFolder->GetFlags(&curFolderFlags);

	  if (uids.get() && (curFolderFlags & MSG_FOLDER_FLAG_IMAPBOX)) 
	  {
	    nsresult rv = NS_OK;
      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_currentFolder);
      nsCOMPtr <nsIURI> uriToSetFlags;
      if (imapFolder)
      {
        rv = imapFolder->SetImapFlags(uids.get(), matchingFlags, getter_AddRefs(uriToSetFlags));
        if (NS_SUCCEEDED(rv) && uriToSetFlags)
        {
          nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(uriToSetFlags);
          if (mailnewsUrl)
            mailnewsUrl->RegisterListener(this);
        }
      }

	  }
  }
  else
    ProcessNextOperation();
}

void
nsImapOfflineSync::ProcessAppendMsgOperation(nsIMsgOfflineImapOperation *currentOp, PRInt32 opType)
{
  nsCOMPtr <nsIMsgDBHdr> mailHdr;
  nsMsgKey msgKey;
  currentOp->GetMessageKey(&msgKey);
  nsresult rv = m_currentDB->GetMsgHdrForKey(msgKey, getter_AddRefs(mailHdr)); 
  if (NS_SUCCEEDED(rv) && mailHdr)
  {
    nsMsgKey messageOffset;
    PRUint32 messageSize;
    mailHdr->GetMessageOffset(&messageOffset);
    mailHdr->GetOfflineMessageSize(&messageSize);
    nsCOMPtr <nsIFileSpec>	tempFileSpec;
    nsSpecialSystemDirectory tmpFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    
    tmpFileSpec += "nscpmsg.txt";
    tmpFileSpec.MakeUnique();
    rv = NS_NewFileSpecWithSpec(tmpFileSpec,
      getter_AddRefs(tempFileSpec));
    if (tempFileSpec)
    {
      nsCOMPtr <nsIOutputStream> outputStream;
      rv = tempFileSpec->GetOutputStream(getter_AddRefs(outputStream));
      if (NS_SUCCEEDED(rv) && outputStream)
      {
        nsXPIDLCString moveDestination;
        currentOp->GetDestinationFolderURI(getter_Copies(moveDestination));
        nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
        nsCOMPtr<nsIRDFResource> res;
        if (NS_FAILED(rv)) return ; // ### return error code.
        rv = rdf->GetResource(moveDestination, getter_AddRefs(res));
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIMsgFolder> destFolder(do_QueryInterface(res, &rv));
          if (NS_SUCCEEDED(rv) && destFolder)
          {
            nsCOMPtr <nsIInputStream> offlineStoreInputStream;
            rv = destFolder->GetOfflineStoreInputStream(getter_AddRefs(offlineStoreInputStream));
            if (NS_SUCCEEDED(rv) && offlineStoreInputStream)
            {
              nsCOMPtr<nsIRandomAccessStore> seekStream = do_QueryInterface(offlineStoreInputStream);
              NS_ASSERTION(seekStream, "non seekable stream - can't read from offline msg");
              if (seekStream)
              {
                rv = seekStream->Seek(PR_SEEK_SET, messageOffset);
                if (NS_SUCCEEDED(rv))
                {
                  // now, copy the dest folder offline store msg to the temp file
                  PRInt32 inputBufferSize = 10240;
                  char *inputBuffer = nsnull;
                  
                  while (!inputBuffer && (inputBufferSize >= 512))
                  {
                    inputBuffer = (char *) PR_Malloc(inputBufferSize);
                    if (!inputBuffer)
                      inputBufferSize /= 2;
                  }
                  PRInt32 bytesLeft;
                  PRUint32 bytesRead, bytesWritten;
                  bytesLeft = messageSize;
                  rv = NS_OK;
                  while (bytesLeft > 0 && NS_SUCCEEDED(rv))
                  {
                    PRInt32 bytesToRead = PR_MIN(inputBufferSize, bytesLeft);
                    rv = offlineStoreInputStream->Read(inputBuffer, bytesToRead, &bytesRead);
                    if (NS_SUCCEEDED(rv) && bytesRead > 0)
                    {
                      rv = outputStream->Write(inputBuffer, bytesRead, &bytesWritten);
                      NS_ASSERTION(bytesWritten == bytesRead, "wrote out correct number of bytes");
                    }
                    else
                      break;
                    bytesLeft -= bytesRead;
                  }
                  outputStream->Flush();
                  tempFileSpec->CloseStream();
                  if (NS_SUCCEEDED(rv))
                  {
                    m_curTempFile = tempFileSpec;
                    rv = destFolder->CopyFileMessage(tempFileSpec,
                      /* nsIMsgDBHdr* msgToReplace */ nsnull,
                      PR_TRUE /* isDraftOrTemplate */,
                      m_window,
                      this);
                  }
                  else
                    m_curTempFile->Delete(PR_FALSE);
                }
                currentOp->ClearOperation(nsIMsgOfflineImapOperation::kAppendDraft);
                m_currentDB->DeleteHeader(mailHdr, nsnull, PR_TRUE, PR_TRUE);
              }
            }
            // want to close in failure case too
            tempFileSpec->CloseStream();
          }
        }
      }
    }
  }
}


void nsImapOfflineSync::ProcessMoveOperation(nsIMsgOfflineImapOperation *currentOp)
{
  nsMsgKeyArray matchingFlagKeys ;
  PRUint32 currentKeyIndex = m_KeyIndex;
  nsXPIDLCString moveDestination;
  currentOp->GetDestinationFolderURI(getter_Copies(moveDestination));
  PRBool moveMatches = PR_TRUE;
  
  do 
  {	// loop for all messsages with the same destination
    if (moveMatches)
    {
      nsMsgKey curKey;
      currentOp->GetMessageKey(&curKey);
      matchingFlagKeys.Add(curKey);
      currentOp->ClearOperation(nsIMsgOfflineImapOperation::kMsgMoved);
    }
    currentOp = nsnull;
    
    if (++currentKeyIndex < m_CurrentKeys.GetSize())
    {
      nsXPIDLCString nextDestination;
      nsresult rv = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[currentKeyIndex], PR_FALSE, &currentOp);
      moveMatches = PR_FALSE;
      if (NS_SUCCEEDED(rv) && currentOp)
      {
        nsOfflineImapOperationType opType; 
        currentOp->GetOperation(&opType);
        if (opType & nsIMsgOfflineImapOperation::kMsgMoved)
        {
          currentOp->GetDestinationFolderURI(getter_Copies(nextDestination));
          moveMatches = nsCRT::strcmp(moveDestination, nextDestination) == 0;
        }
      }
    }
  } 
  while (currentOp);
  
  nsresult rv;
  
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) return ; // ### return error code.
  rv = rdf->GetResource(moveDestination, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> destFolder(do_QueryInterface(res, &rv));
    if (NS_SUCCEEDED(rv) && destFolder)
    {
      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_currentFolder);
      if (imapFolder)
      {
        rv = imapFolder->ReplayOfflineMoveCopy(matchingFlagKeys.GetArray(), matchingFlagKeys.GetSize(), PR_TRUE, destFolder,
          this, m_window);
      }
      else
      {
        nsCOMPtr <nsISupportsArray> messages = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
        if (messages && NS_SUCCEEDED(rv))
        {
          NS_NewISupportsArray(getter_AddRefs(messages));
          for (PRUint32 keyIndex = 0; keyIndex < matchingFlagKeys.GetSize(); keyIndex++)
          {
            nsCOMPtr<nsIMsgDBHdr> mailHdr = nsnull;
            rv = m_currentFolder->GetMessageHeader(matchingFlagKeys.ElementAt(keyIndex), getter_AddRefs(mailHdr));
            if (NS_SUCCEEDED(rv) && mailHdr)
            {
              nsCOMPtr<nsISupports> iSupports;
              iSupports = do_QueryInterface(mailHdr);
              messages->AppendElement(iSupports);
            }
          }
          destFolder->CopyMessages(m_currentFolder, messages, PR_TRUE, m_window, this, PR_FALSE, PR_FALSE);
        }
      }
    }
  }
}


void nsImapOfflineSync::ProcessCopyOperation(nsIMsgOfflineImapOperation *currentOp)
{
  nsMsgKeyArray matchingFlagKeys;
  PRUint32 currentKeyIndex = m_KeyIndex;
  nsXPIDLCString copyDestination;
  currentOp->GetCopyDestination(0, getter_Copies(copyDestination));
  PRBool copyMatches = PR_TRUE;
  
  do {	// loop for all messsages with the same destination
    if (copyMatches)
    {
      nsMsgKey curKey;
      currentOp->GetMessageKey(&curKey);
      matchingFlagKeys.Add(curKey);
      currentOp->ClearOperation(nsIMsgOfflineImapOperation::kMsgCopy);
    }
    currentOp = nsnull;
    
    if (++currentKeyIndex < m_CurrentKeys.GetSize())
    {
      nsXPIDLCString nextDestination;
      nsresult rv = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[currentKeyIndex], PR_FALSE, &currentOp);
      copyMatches = PR_FALSE;
      if (NS_SUCCEEDED(rv) && currentOp)
      {
        nsOfflineImapOperationType opType; 
        currentOp->GetOperation(&opType);
        if (opType & nsIMsgOfflineImapOperation::kMsgCopy)
        {
          currentOp->GetCopyDestination(0, getter_Copies(nextDestination));
          copyMatches = nsCRT::strcmp(copyDestination, nextDestination) == 0;
        }
      }
    }
  } 
  while (currentOp);
	
  nsCAutoString uids;

  nsresult rv;

  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) return ; // ### return error code.
  rv = rdf->GetResource(copyDestination, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> destFolder(do_QueryInterface(res, &rv));
    if (NS_SUCCEEDED(rv) && destFolder)
    {
      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_currentFolder);
      if (imapFolder)
      {
        rv = imapFolder->ReplayOfflineMoveCopy(matchingFlagKeys.GetArray(), matchingFlagKeys.GetSize(), PR_FALSE, destFolder,
                       this, m_window);
      }
      else
      {
        nsCOMPtr <nsISupportsArray> messages = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
        if (messages && NS_SUCCEEDED(rv))
        {
          NS_NewISupportsArray(getter_AddRefs(messages));
          for (PRUint32 keyIndex = 0; keyIndex < matchingFlagKeys.GetSize(); keyIndex++)
          {
            nsCOMPtr<nsIMsgDBHdr> mailHdr = nsnull;
            rv = m_currentFolder->GetMessageHeader(matchingFlagKeys.ElementAt(keyIndex), getter_AddRefs(mailHdr));
            if (NS_SUCCEEDED(rv) && mailHdr)
            {
              nsCOMPtr<nsISupports> iSupports;
              iSupports = do_QueryInterface(mailHdr);
              messages->AppendElement(iSupports);
            }
          }
          destFolder->CopyMessages(m_currentFolder, messages, PR_FALSE, m_window, this, PR_FALSE, PR_FALSE);
        }
      }
    }
  }
}

void nsImapOfflineSync::ProcessEmptyTrash(nsIMsgOfflineImapOperation *currentOp)
{
	m_currentFolder->EmptyTrash(m_window, this);
	m_currentDB->RemoveOfflineOp(currentOp);
	m_currentDB = nsnull;	// empty trash deletes the database?
}

// returns PR_TRUE if we found a folder to create, PR_FALSE if we're done creating folders.
PRBool nsImapOfflineSync::CreateOfflineFolders()
{
	while (m_currentFolder)
	{
		PRUint32 flags;
    m_currentFolder->GetFlags(&flags);
		PRBool offlineCreate = (flags & MSG_FOLDER_FLAG_CREATED_OFFLINE) != 0;
		if (offlineCreate)
		{
			if (CreateOfflineFolder(m_currentFolder))
				return PR_TRUE;
		}
		AdvanceToNextFolder();
	}
	return PR_FALSE;
}

PRBool nsImapOfflineSync::CreateOfflineFolder(nsIMsgFolder *folder)
{
  nsCOMPtr<nsIFolder> parent;
  folder->GetParent(getter_AddRefs(parent));

  nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(parent);
  nsCOMPtr <nsIURI> createFolderURI;
   nsXPIDLCString onlineName;
  imapFolder->GetOnlineName(getter_Copies(onlineName));

  NS_ConvertASCIItoUCS2 folderName(onlineName);
//  folderName.AssignWithConversion(onlineName);
	nsresult rv = imapFolder->PlaybackOfflineFolderCreate(folderName.get(), nsnull,  getter_AddRefs(createFolderURI));
  if (createFolderURI && NS_SUCCEEDED(rv))
  {
    nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(createFolderURI);
    if (mailnewsUrl)
      mailnewsUrl->RegisterListener(this);
  }
  return NS_SUCCEEDED(rv) ? PR_TRUE : PR_FALSE;	// this is asynch, we have to return and be called again by the OfflineOpExitFunction
}

PRInt32 nsImapOfflineSync::GetCurrentUIDValidity()
{
   uid_validity_info uidStruct;

  if (m_currentFolder)
  {
    nsCOMPtr <nsIImapMiscellaneousSink> miscellaneousSink = do_QueryInterface(m_currentFolder);
    if (miscellaneousSink)
    {
      uidStruct.returnValidity = kUidUnknown;
      miscellaneousSink->GetStoredUIDValidity(nsnull, &uidStruct);
      mCurrentUIDValidity = uidStruct.returnValidity;    
    }
  }
  return mCurrentUIDValidity; 
}

// Playing back offline operations is one giant state machine that runs through ProcessNextOperation.
// The first state is creating online any folders created offline (we do this first, so we can play back
// any operations in them in the next pass)

nsresult nsImapOfflineSync::ProcessNextOperation()
{
  nsresult rv = NS_OK;
  // find a folder that needs to process operations
  nsIMsgFolder	*deletedAllOfflineEventsInFolder = nsnull;
  
  // if we haven't created offline folders, and we're updating all folders,
  // first, find offline folders to create.
  if (!m_createdOfflineFolders)
  {
    if (m_singleFolderToUpdate)
    {
      if (!m_pseudoOffline)
      {
        AdvanceToFirstIMAPFolder();
        if (CreateOfflineFolders())
          return NS_OK;
      }	
    }
    else
    {
      if (CreateOfflineFolders())
        return NS_OK;
      AdvanceToFirstIMAPFolder();
    }
    m_createdOfflineFolders = PR_TRUE;
  }
  // if updating one folder only, restore m_currentFolder to that folder
  if (m_singleFolderToUpdate)
    m_currentFolder = m_singleFolderToUpdate;
  
  PRUint32 folderFlags;
  nsCOMPtr <nsIDBFolderInfo> folderInfo;
  while (m_currentFolder && !m_currentDB)
  {
    m_currentFolder->GetFlags(&folderFlags);
    // need to check if folder has offline events, or is configured for offline
    if (folderFlags & (MSG_FOLDER_FLAG_OFFLINEEVENTS | MSG_FOLDER_FLAG_OFFLINE))
    {
      m_currentFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(m_currentDB));
    }
    if (m_currentDB)
    {
      m_CurrentKeys.RemoveAll();
      m_KeyIndex = 0;
      if ((m_currentDB->ListAllOfflineOpIds(&m_CurrentKeys) != 0) || !m_CurrentKeys.GetSize())
      {
        m_currentDB = nsnull;
        m_currentFolder->ClearFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
      }
      else
      {
        // trash any ghost msgs
        PRBool deletedGhostMsgs = PR_FALSE;
        for (PRUint32 fakeIndex=0; fakeIndex < m_CurrentKeys.GetSize(); fakeIndex++)
        {
          nsCOMPtr <nsIMsgOfflineImapOperation> currentOp; 
          m_currentDB->GetOfflineOpForKey(m_CurrentKeys[fakeIndex], PR_FALSE, getter_AddRefs(currentOp));
          if (currentOp)
          {
            nsOfflineImapOperationType opType; 
            currentOp->GetOperation(&opType);
            
            if (opType == nsIMsgOfflineImapOperation::kMoveResult)
            {
              nsMsgKey curKey;
              currentOp->GetMessageKey(&curKey);
              m_currentDB->RemoveOfflineOp(currentOp);
              deletedGhostMsgs = PR_TRUE;
              
              nsCOMPtr <nsIMsgDBHdr> mailHdr;
              m_currentDB->DeleteMessage(curKey, nsnull, PR_FALSE);
            }
          }
        }
        
        if (deletedGhostMsgs)
          m_currentFolder->SummaryChanged();
        
        m_CurrentKeys.RemoveAll();
        if ( (m_currentDB->ListAllOfflineOpIds(&m_CurrentKeys) != 0) || !m_CurrentKeys.GetSize() )
        {
          m_currentDB = nsnull;
          if (deletedGhostMsgs)
            deletedAllOfflineEventsInFolder = m_currentFolder;
        }
        else if (folderFlags & MSG_FOLDER_FLAG_IMAPBOX)
        {
          //					if (imapFolder->GetHasOfflineEvents())
          //						XP_ASSERT(PR_FALSE);
          
          if (!m_pseudoOffline)	// if pseudo offline, falls through to playing ops back.
          {
            // there are operations to playback so check uid validity
            SetCurrentUIDValidity(0);	// force initial invalid state
            // do a lite select here and hook ourselves up as a listener.
            nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_currentFolder, &rv);
            if (imapFolder)
              rv = imapFolder->LiteSelect(this);
            return rv;	// this is async, we have to return as be called again by the OnStopRunningUrl
          }
        }
      }
    }
    
    if (!m_currentDB)
    {
      // only advance if we are doing all folders
      if (!m_singleFolderToUpdate)
        AdvanceToNextFolder();
      else
        m_currentFolder = nsnull;	// force update of this folder now.
    }
    
  }
  
  if (m_currentFolder)
    m_currentFolder->GetFlags(&folderFlags);
  // do the current operation
  if (m_currentDB)
  {	
    PRBool currentFolderFinished = PR_FALSE;
    if (!folderInfo)
      m_currentDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
    // user canceled the lite select! if GetCurrentUIDValidity() == 0
    if ((m_KeyIndex < m_CurrentKeys.GetSize()) && (m_pseudoOffline || (GetCurrentUIDValidity() != 0) || !(folderFlags & MSG_FOLDER_FLAG_IMAPBOX)) )
    {
      PRInt32 curFolderUidValidity;
      folderInfo->GetImapUidValidity(&curFolderUidValidity);
      PRBool uidvalidityChanged = (!m_pseudoOffline && folderFlags & MSG_FOLDER_FLAG_IMAPBOX) && (GetCurrentUIDValidity() != curFolderUidValidity);
      nsIMsgOfflineImapOperation *currentOp = nsnull;
      if (uidvalidityChanged)
        DeleteAllOfflineOpsForCurrentDB();
      else
        m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, &currentOp);
      
      if (currentOp)
      {
        nsOfflineImapOperationType opType; 
        
        if (currentOp)
          currentOp->GetOperation(&opType);
        // loop until we find the next db record that matches the current playback operation
        while (currentOp && !(opType & mCurrentPlaybackOpType))
        {
          currentOp = nsnull;
          ++m_KeyIndex;
          if (m_KeyIndex < m_CurrentKeys.GetSize())
            m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, &currentOp);
          if (currentOp)
            currentOp->GetOperation(&opType);
        }
        
        // if we did not find a db record that matches the current playback operation,
        // then move to the next playback operation and recurse.  
        if (!currentOp)
        {
          // we are done with the current type
          if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kFlagsChanged)
          {
            mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kMsgCopy;
            // recurse to deal with next type of operation
            m_KeyIndex = 0;
            ProcessNextOperation();
          }
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kMsgCopy)
          {
            mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kMsgMoved;
            // recurse to deal with next type of operation
            m_KeyIndex = 0;
            ProcessNextOperation();
          }
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kMsgMoved)
          {
            mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kAppendDraft;
            // recurse to deal with next type of operation
            m_KeyIndex = 0;
            ProcessNextOperation();
          }
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kAppendDraft)
          {
            mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kAppendTemplate;
            // recurse to deal with next type of operation
            m_KeyIndex = 0;
            ProcessNextOperation();
          }
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kAppendTemplate)
          {
            mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kDeleteAllMsgs;
            m_KeyIndex = 0;
            ProcessNextOperation();
          }
          else
          {
            DeleteAllOfflineOpsForCurrentDB();
            currentFolderFinished = PR_TRUE;
          }
          
        }
        else
        {
          if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kFlagsChanged)
            ProcessFlagOperation(currentOp);
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kMsgCopy)
            ProcessCopyOperation(currentOp);
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kMsgMoved)
            ProcessMoveOperation(currentOp);
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kAppendDraft)
            ProcessAppendMsgOperation(currentOp, nsIMsgOfflineImapOperation::kAppendDraft);
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kAppendTemplate)
            ProcessAppendMsgOperation(currentOp, nsIMsgOfflineImapOperation::kAppendTemplate);
          else if (mCurrentPlaybackOpType == nsIMsgOfflineImapOperation::kDeleteAllMsgs)
            ProcessEmptyTrash(currentOp);
          else
            NS_ASSERTION(PR_FALSE, "invalid playback op type");
          // currentOp was unreferred by one of the Process functions
          // so do not reference it again!
          currentOp = nsnull;
        }
      }
      else
        currentFolderFinished = PR_TRUE;
    }
    else
      currentFolderFinished = PR_TRUE;
    
    if (currentFolderFinished)
    {
      m_currentDB = nsnull;
      if (!m_singleFolderToUpdate)
      {
        AdvanceToNextFolder();
        ProcessNextOperation();
        return NS_OK;
      }
      else
        m_currentFolder = nsnull;
    }
  }
  
  if (!m_currentFolder && !m_mailboxupdatesStarted)
  {
    m_mailboxupdatesStarted = PR_TRUE;
    
    // if we are updating more than one folder then we need the iterator
    if (!m_singleFolderToUpdate)
      AdvanceToFirstIMAPFolder();
    if (m_singleFolderToUpdate)
    {
      m_singleFolderToUpdate->ClearFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
      m_singleFolderToUpdate->UpdateFolder(m_window);
      // do we have to do anything? Old code would do a start update...
    }
    else
    {
    }
    
    //		MSG_FolderIterator *updateFolderIterator = m_singleFolderToUpdate ? (MSG_FolderIterator *) 0 : m_folderIterator;
    
    
    // we are done playing commands back, now queue up the sync with each imap folder
    // If we're using the iterator, m_currentFolder will be set correctly
    //			nsIMsgFolder * folder = m_singleFolderToUpdate ? m_singleFolderToUpdate : m_currentFolder;
    //			while (folder)
    //			{            
    //				PRBool loadingFolder = m_workerPane->GetLoadingImapFolder() == folder;
    //				if ((folder->GetType() == FOLDER_IMAPMAIL) && (deletedAllOfflineEventsInFolder == folder || (folder->GetFolderPrefFlags() & MSG_FOLDER_FLAG_OFFLINE)
    //					|| loadingFolder) 
    //					&& !(folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT) )
    //				{
    //					PRBool lastChance = ((deletedAllOfflineEventsInFolder == folder) && m_singleFolderToUpdate) || loadingFolder;
    // if deletedAllOfflineEventsInFolder == folder and we're only updating one folder, then we need to update newly selected folder
    // I think this also means that we're really opening the folder...so we tell StartUpdate that we're loading a folder.
    //					if (!updateFolderIterator || !(imapMail->GetFlags() & MSG_FOLDER_FLAG_INBOX))		// avoid queueing the inbox twice
    //						imapMail->StartUpdateOfNewlySelectedFolder(m_workerPane, lastChance, queue, nsnsnull, PR_FALSE, PR_FALSE);
    //				}
    //				folder= m_singleFolderToUpdate ? (MSG_FolderInfo *)nsnull : updateFolderIterator->Next();
    //       }
  }
  // if we get here, then I *think* we're done. Not sure, though.
#ifdef DEBUG_bienvenu
  printf("done with offline imap sync\n");
#endif
  nsCOMPtr <nsIUrlListener> saveListener = m_listener;
  m_listener = nsnull;

  if (saveListener)
    saveListener->OnStopRunningUrl(nsnull /* don't know url */, rv);
  return rv;
}


void nsImapOfflineSync::DeleteAllOfflineOpsForCurrentDB()
{
  m_KeyIndex = 0;
  nsCOMPtr <nsIMsgOfflineImapOperation> currentOp;
  m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, getter_AddRefs(currentOp));
  while (currentOp)
  {
    // NS_ASSERTION(currentOp->GetOperationFlags() == 0);
    // delete any ops that have already played back
    m_currentDB->RemoveOfflineOp(currentOp);
    m_currentDB->Commit(nsMsgDBCommitType::kLargeCommit);
    currentOp = nsnull;
    
    if (++m_KeyIndex < m_CurrentKeys.GetSize())
      m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, getter_AddRefs(currentOp));
  }
  // turn off MSG_FOLDER_PREF_OFFLINEEVENTS
  if (m_currentFolder)
    m_currentFolder->ClearFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
}

nsImapOfflineDownloader::nsImapOfflineDownloader(nsIMsgWindow *aMsgWindow, nsIUrlListener *aListener) : nsImapOfflineSync(aMsgWindow, aListener)
{
}

nsImapOfflineDownloader::~nsImapOfflineDownloader()
{
}

nsresult nsImapOfflineDownloader::ProcessNextOperation()
{
  nsresult rv = NS_OK;
  if (!m_mailboxupdatesStarted)
  {
    m_mailboxupdatesStarted = PR_TRUE;
    // Update the INBOX first so the updates on the remaining
    // folders pickup the results of any filter moves.
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(kMsgAccountManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsISupportsArray> servers;
  
    rv = accountManager->GetAllServers(getter_AddRefs(servers));
    if (NS_FAILED(rv)) return rv;
  }
  if (!m_mailboxupdatesFinished)
  {
    AdvanceToNextServer();
    if (m_currentServer)
    {
      nsCOMPtr <nsIFolder> rootFolder;
      m_currentServer->GetRootFolder(getter_AddRefs(rootFolder));
      nsCOMPtr<nsIMsgFolder> inbox;
      if (rootFolder)
      {
        nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
        if (rootMsgFolder)
        {
          PRUint32 numFolders;
          rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inbox));
          if (inbox)
          {
            rv = inbox->GetNewMessages(m_window, this);
            if (NS_SUCCEEDED(rv))
              return rv; // otherwise, fall through.
          }
        }
      }
      return ProcessNextOperation(); // recurse and do next server.
    }
    else
    {
      m_allServers = nsnull;
      m_mailboxupdatesFinished = PR_TRUE;
    }
  }
  AdvanceToNextFolder();

	while (m_currentFolder)
	{
    PRUint32 folderFlags;

    m_currentDB = nsnull;
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder;
    if (m_currentFolder)
      imapFolder = do_QueryInterface(m_currentFolder);
    m_currentFolder->GetFlags(&folderFlags);
		// need to check if folder has offline events, or is configured for offline
		if (imapFolder && folderFlags & MSG_FOLDER_FLAG_OFFLINE)
      return m_currentFolder->DownloadAllForOffline(this, m_window);
    else
      AdvanceToNextFolder();
  }
  if (m_listener)
    m_listener->OnStopRunningUrl(nsnull, NS_OK);
  return rv;
}


NS_IMETHODIMP nsImapOfflineSync::OnStartCopy()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void OnProgress (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP nsImapOfflineSync::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetMessageKey (in PRUint32 aKey); */
NS_IMETHODIMP nsImapOfflineSync::SetMessageKey(PRUint32 aKey)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void GetMessageId (in nsCString aMessageId); */
NS_IMETHODIMP nsImapOfflineSync::GetMessageId(nsCString * aMessageId)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void OnStopCopy (in nsresult aStatus); */
NS_IMETHODIMP nsImapOfflineSync::OnStopCopy(nsresult aStatus)
{
  return OnStopRunningUrl(nsnull, aStatus);
}



