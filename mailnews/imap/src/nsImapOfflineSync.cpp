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

static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

NS_IMPL_ISUPPORTS1(nsImapOfflineSync, nsIUrlListener)

nsImapOfflineSync::nsImapOfflineSync(nsIMsgFolder *singleFolderOnly)
{
  NS_INIT_REFCNT();
  m_singleFolderToUpdate = singleFolderOnly;
}

NS_IMETHODIMP nsImapOfflineSync::OnStartRunningUrl(nsIURI* url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsImapOfflineSync::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  nsresult rv = exitCode;
  if (NS_SUCCEEDED(exitCode))
    rv = ProcessNextOperation();

  return rv;
}


void nsImapOfflineSync::AdvanceToNextFolder()
{
	// we always start by changing flags
  mCurrentPlaybackOpType = nsIMsgOfflineImapOperation::kFlagsChanged;
	
  // use GetAllServers to get all the servers, then get the root folder,
  // and iterate over all the folders using GetSubfolders.
}

void nsImapOfflineSync::AdvanceToFirstIMAPFolder()
{
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
    currentOp->GetFlagOperation(&flagOperation);
    currentOp->GetNewFlags(&newFlags);
	} while (currentOp && (flagOperation & nsIMsgOfflineImapOperation::kFlagsChanged) && (newFlags == matchingFlags) );
	
  currentOp = nsnull;
	
  nsCAutoString uids;
	nsImapMailFolder::AllocateUidStringFromKeyArray(matchingFlagKeys, uids);
  PRUint32 curFolderFlags;
  m_currentFolder->GetFlags(&curFolderFlags);

	if (uids && (curFolderFlags & MSG_FOLDER_FLAG_IMAPBOX)) 
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

void
nsImapOfflineSync::ProcessAppendMsgOperation(nsIMsgOfflineImapOperation *currentOp, PRInt32 opType)
{
	nsCOMPtr <nsIMsgDBHdr> mailHdr;
  nsMsgKey msgKey;
  currentOp->GetMessageKey(&msgKey);
  nsresult rv = m_currentDB->GetMsgHdrForKey(msgKey, getter_AddRefs(mailHdr)); 
#ifdef NOT_IMPL_YET
	if (NS_SUCCEEDED(rv) && mailHdr)
	{
		char *msg_file_name = WH_TempName (xpFileToPost, "nsmail");
		if (msg_file_name)
		{
			XP_File msg_file = XP_FileOpen(msg_file_name, xpFileToPost,
										   XP_FILE_WRITE_BIN);
			if (msg_file)
			{
				mailHdr->WriteOfflineMessage(msg_file, m_currentDB->GetDB(), PR_FALSE);
				XP_FileClose(msg_file);
				nsCAutoString moveDestination;
				currentOp->GetMoveDestination(moveDestination);
				MSG_IMAPFolderInfoMail *currentIMAPFolder = m_currentFolder->GetIMAPFolderInfoMail();

				MSG_IMAPFolderInfoMail *mailFolderInfo = currentIMAPFolder
					? m_workerPane->GetMaster()->FindImapMailFolder(currentIMAPFolder->GetHostName(), moveDestination, nsnsnull, PR_FALSE)
					: m_workerPane->GetMaster()->FindImapMailFolder(moveDestination);
				char *urlString = 
					CreateImapAppendMessageFromFileUrl(
						mailFolderInfo->GetHostName(),
						mailFolderInfo->GetOnlineName(),
						mailFolderInfo->GetOnlineHierarchySeparator(),
						opType == kAppendDraft);
				if (urlString)
				{
					URL_Struct *url = NET_CreateURLStruct(urlString,
														  NET_NORMAL_RELOAD); 
					if (url)
					{
						url->post_data = XP_STRDUP(msg_file_name);
						url->post_data_size = XP_STRLEN(msg_file_name);
						url->post_data_is_file = PR_TRUE;
						url->method = URL_POST_METHOD;
						url->fe_data = (void *) new
							AppendMsgOfflineImapState(
								mailFolderInfo,
								currentOp->GetMessageKey(), msg_file_name);
						url->internal_url = PR_TRUE;
						url->msg_pane = m_workerPane;
						m_workerPane->GetContext()->imapURLPane = m_workerPane;
						MSG_UrlQueue::AddUrlToPane (url,
													PostAppendMsgExitFunction,
													m_workerPane, PR_TRUE);
						currentOp->ClearAppendMsgOperation(opType);
					}
					XP_FREEIF(urlString);
				}
			}
			XP_FREEIF(msg_file_name);
		}
		mailHdr->unrefer();
	}
#endif // NOT_IMPL_YET
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
	
  nsCAutoString uids;
	nsImapMailFolder::AllocateUidStringFromKeyArray(matchingFlagKeys, uids);

  nsresult rv;

  nsCOMPtr<nsIRDFResource> res;
  NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) return ; // ### return error code.
  rv = rdf->GetResource(moveDestination, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> destFolder(do_QueryInterface(res, &rv));
    if (NS_SUCCEEDED(rv) && destFolder)
    {
      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(destFolder);
      if (imapFolder)
        rv = imapFolder->ReplayOfflineMoveCopy(uids.get(), PR_TRUE, destFolder,
                       this, nsnull);
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
        	currentOp->GetCopyDestination(0, getter_Copies(copyDestination));
          copyMatches = nsCRT::strcmp(copyDestination, nextDestination) == 0;
        }
      }
		}
	} 
  while (currentOp);
	
  nsCAutoString uids;
	nsImapMailFolder::AllocateUidStringFromKeyArray(matchingFlagKeys, uids);

  nsresult rv;

  nsCOMPtr<nsIRDFResource> res;
  NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) return ; // ### return error code.
  rv = rdf->GetResource(copyDestination, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> destFolder(do_QueryInterface(res, &rv));
    if (NS_SUCCEEDED(rv) && destFolder)
    {
      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(destFolder);
      if (imapFolder)
        rv = imapFolder->ReplayOfflineMoveCopy(uids.get(), PR_FALSE, destFolder,
                       this, nsnull);
    }
	}
}

void nsImapOfflineSync::ProcessEmptyTrash(nsIMsgOfflineImapOperation *currentOp)
{
#ifdef NOT_IMPL_YET
	currentOp->unrefer();
	MSG_IMAPFolderInfoMail *currentIMAPFolder = m_currentFolder->GetIMAPFolderInfoMail();
	char *trashUrl = CreateImapDeleteAllMessagesUrl(currentIMAPFolder->GetHostName(), 
		                                            currentIMAPFolder->GetOnlineName(),
		                                            currentIMAPFolder->GetOnlineHierarchySeparator());
	// we're not going to delete sub-folders, since that prompts the user, a no-no while synchronizing.
	if (trashUrl)
	{
		queue->AddUrl(trashUrl, OfflineOpExitFunction);
		if (!alreadyRunningQueue)
			queue->GetNextUrl();	
		m_currentDB->DeleteOfflineOp(currentOp->GetMessageKey());

		m_currentDB = nsnull;	// empty trash deletes the database?
	}
#endif // NOT_IMPL_YET
}

// returns PR_TRUE if we found a folder to create, PR_FALSE if we're done creating folders.
PRBool nsImapOfflineSync::CreateOfflineFolders()
{
#ifdef NOT_IMPL_YET
	while (m_currentFolder)
	{
		int32 prefFlags = m_currentFolder->GetFolderPrefFlags();
		PRBool offlineCreate = (prefFlags & MSG_FOLDER_PREF_CREATED_OFFLINE) != 0;
		if (offlineCreate)
		{
			if (CreateOfflineFolder(m_currentFolder))
				return PR_TRUE;
		}
		AdvanceToNextFolder();
	}
#endif
	return PR_FALSE;
}

PRBool nsImapOfflineSync::CreateOfflineFolder(nsIMsgFolder *folder)
{
#ifdef NOT_IMPL_YET
	MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
	char *url = CreateImapMailboxCreateUrl(imapFolder->GetHostName(), imapFolder->GetOnlineName(), imapFolder->GetOnlineHierarchySeparator());
	if (url)
	{
		PRBool alreadyRunningQueue;
		MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);
		if (queue)
		{
			// should we insert this at 0, or add? I think we want to run offline events
			// before any new events...but this is just a lite select
			queue->AddUrl(url, OfflineOpExitFunction);
			if (!alreadyRunningQueue)
				queue->GetNextUrl();	
			return PR_TRUE;	// this is asynch, we have to return and be called again by the OfflineOpExitFunction
		}
	}
#endif
	return PR_FALSE;
}

// Playing back offline operations is one giant state machine that runs through ProcessNextOperation.
// The first state is creating online any folders created offline (we do this first, so we can play back
// any operations in them in the next pass)

nsresult nsImapOfflineSync::ProcessNextOperation()
{
  nsresult rv;
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
						  m_currentDB->RemoveOfflineOp(currentOp);
						  deletedGhostMsgs = PR_TRUE;
						  
						  nsCOMPtr <nsIMsgDBHdr> mailHdr;
              nsMsgKey curKey;
              currentOp->GetMessageKey(&curKey);
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
            // ### do a lite select here and hook ourselves up as a listener.
  					return NS_OK;	// this is asynch, we have to return as be called again by the OfflineOpExitFunction
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
	
  m_currentFolder->GetFlags(&folderFlags);
	// do the current operation
	if (m_currentDB)
	{	
		PRBool currentFolderFinished = PR_FALSE;
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

				// loop until we find the next db record that matches the current playback operation
				while (currentOp)
        {
          currentOp->GetOperation(&opType);
          if (opType & mCurrentPlaybackOpType)
				  {
					  currentOp = nsnull;
					  if (++m_KeyIndex < m_CurrentKeys.GetSize())
						  m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, &currentOp);
				  }
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
      // do we have to do anything? Old code would do a start update...
    }
    else
    {
			// this means that we are updating all of the folders.  Update the INBOX first so the updates on the remaining
			// folders pickup the results of any filter moves.
//			nsIMsgFolder *inboxFolder;
			if (!m_pseudoOffline )
			{
 	      NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, kMsgAccountManagerCID, &rv);
	      if (NS_FAILED(rv)) return rv;
	      nsCOMPtr<nsISupportsArray> servers;
	      
	      rv = accountManager->GetAllServers(getter_AddRefs(servers));
	      if (NS_FAILED(rv)) return rv;
        // ### for each imap server, call get new messages.
      // get next folder...
      }
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
  return rv;
}


void nsImapOfflineSync::DeleteAllOfflineOpsForCurrentDB()
{
	m_KeyIndex = 0;
	nsCOMPtr <nsIMsgOfflineImapOperation> currentOp;
  m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, getter_AddRefs(currentOp));
	while (currentOp)
	{
//		NS_ASSERTION(currentOp->GetOperationFlags() == 0);
		// delete any ops that have already played back
		m_currentDB->RemoveOfflineOp(currentOp);
		currentOp = nsnull;
		
		if (++m_KeyIndex < m_CurrentKeys.GetSize())
			m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], PR_FALSE, getter_AddRefs(currentOp));
	}
	// turn off MSG_FOLDER_PREF_OFFLINEEVENTS
	if (m_currentFolder)
		m_currentFolder->ClearFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
}
