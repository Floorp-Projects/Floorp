/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* offline imap download stream */
#include "msg.h"
#include "imapoff.h"
#include "maildb.h"
#include "msgfinfo.h"
#include "mailhdr.h"
#include "msgpane.h"
#include "msgprefs.h"
#include "msgurlq.h"
#include "xpgetstr.h"
#include "prefapi.h"
#include "imap.h"
#include "grpinfo.h"
#include "msgdbapi.h"
#include "msgstrob.h"

extern "C"
{
    extern int MK_MSG_CONFIRM_CONTINUE_IMAP_SYNC;
    extern int MK_MSG_HTML_IMAP_NO_CACHED_BODY;
}	// extern "C"



// lame forward to kill mac warning
void OffOnlineSynchExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);

// when we get here, we're done, give the other deferred tasks a chance now
void OffOnlineSynchExitFunction (URL_Struct *URL_s, int /*status*/, MWContext *)
{
	OfflineImapGoOnlineState *goState = URL_s->msg_pane->GetGoOnlineState();
	if (goState)
	{
		delete goState;
		URL_s->msg_pane->SetGoOnlineState(NULL);
	}
	
	// kick off any other on/off urls (like news synching)
	if (URL_s->msg_pane && URL_s->msg_pane->GetURLChain())
		URL_s->msg_pane->GetURLChain()->GetNextURL();
}

// lame forward to kill mac warning
void OfflineOpExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);

void OfflineOpExitFunction (URL_Struct *URL_s, int status, MWContext *window_id)
{
	OfflineImapGoOnlineState *goState = URL_s->msg_pane->GetGoOnlineState();
	if (goState)
	{
#ifdef DEBUG_bienvenu
		XP_ASSERT(status != MK_INTERRUPTED);	// unless user interrupted, don't want to see this.
#endif
		if (status != MK_INTERRUPTED && ((status >= 0) || FE_Confirm(URL_s->msg_pane->GetContext(), XP_GetString(MK_MSG_CONFIRM_CONTINUE_IMAP_SYNC) )))
			goState->ProcessNextOperation();
		else
		{
			// This function will clear the pane of any information about 
			// folder loads and send a MSG_PaneNotifyFolderLoaded is necessary
			if (goState->ProcessingStaleFolderUpdate())
				ImapFolderSelectCompleteExitFunction(URL_s, status, window_id);
				
			delete goState;
			URL_s->msg_pane->SetGoOnlineState(NULL);
		}
	}
}

void ReportLiteSelectUIDVALIDITY(MSG_Pane *receivingPane, uint32 UIDVALIDITY)
{
	OfflineImapGoOnlineState *goState = receivingPane ? receivingPane->GetGoOnlineState() : (OfflineImapGoOnlineState *)NULL;
	if (goState)
		goState->SetCurrentUIDValidity(UIDVALIDITY);
}


OfflineImapGoOnlineState::OfflineImapGoOnlineState(MSG_Pane *workerPane, MSG_IMAPFolderInfoMail *singleFolderOnly)
{
	workerPane->SetGoOnlineState(this);
	workerPane->GetMaster()->SetPlayingBackOfflineOps(TRUE);
	m_workerPane = workerPane;
	m_mailboxupdatesStarted = FALSE;
	mCurrentPlaybackOpType = kFlagsChanged;
	m_pseudoOffline = FALSE;
	m_createdOfflineFolders = FALSE;
	
	m_singleFolderToUpdate = singleFolderOnly;
	
	// start with the first imap mailbox
	if (!m_singleFolderToUpdate)
	{
		m_folderIterator = new MSG_FolderIterator(workerPane->GetMaster()->GetImapMailFolderTree());
		m_currentFolder = (MSG_FolderInfoMail *) m_folderIterator->First();
		if (m_currentFolder && (m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAP_SERVER))
			AdvanceToNextFolder();
	}
	else
		m_currentFolder = m_singleFolderToUpdate;
	m_urlQueue = NULL;
	m_downloadOfflineImapState = NULL;
}

OfflineImapGoOnlineState::~OfflineImapGoOnlineState()
{
	if (m_currentDB)
		m_currentDB->Close();
	m_workerPane->GetMaster()->SetPlayingBackOfflineOps(FALSE);
	delete m_folderIterator;
	delete m_downloadOfflineImapState;
}

MSG_UrlQueue *OfflineImapGoOnlineState::GetUrlQueue(XP_Bool *alreadyRunning)
{
	MSG_UrlQueue *queue = MSG_UrlQueue::FindQueue (m_workerPane);
	*alreadyRunning = queue != NULL;

	if (!queue)	// should this be a MSG_ImapLoadFolderUrlQueue?	  I think so.
		queue = new MSG_ImapLoadFolderUrlQueue(m_workerPane);
	else
		XP_Trace("we're using another queue for playing back an online operation!\n");

	queue->AddInterruptCallback(MSG_UrlQueue::HandleFolderLoadInterrupt);

	return queue;
}
	
void OfflineImapGoOnlineState::AdvanceToNextFolder()
{
	// we always start by changing flags
	mCurrentPlaybackOpType = kFlagsChanged;
	
	m_currentFolder = (MSG_FolderInfoMail *) m_folderIterator->Next();
	if (m_currentFolder && (m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAP_SERVER))
		AdvanceToNextFolder();
	
	if (!m_currentFolder && (m_folderIterator->m_masterParent == m_workerPane->GetMaster()->GetImapMailFolderTree()))
	{
		delete m_folderIterator;
		m_folderIterator = new MSG_FolderIterator(m_workerPane->GetMaster()->GetLocalMailFolderTree());
			m_currentFolder = (MSG_FolderInfoMail *) m_folderIterator->First();
	}
}

void OfflineImapGoOnlineState::ProcessFlagOperation(DBOfflineImapOperation *currentOp)
{
	IDArray matchingFlagKeys;
	int currentKeyIndex = m_KeyIndex;
	imapMessageFlagsType matchingFlags = currentOp->GetNewMessageFlags();
	
	do {	// loop for all messsages with the same flags
			matchingFlagKeys.Add(currentOp->GetMessageKey());
			currentOp->ClearImapFlagOperation();
			delete currentOp;
			currentOp = NULL;
			if (++currentKeyIndex < m_CurrentKeys.GetSize())
				currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[currentKeyIndex], FALSE);
	} while (currentOp && (currentOp->GetOperationFlags() & kFlagsChanged) && (currentOp->GetNewMessageFlags() == matchingFlags) );
	
	if (currentOp)
	{
		delete currentOp;
		currentOp = NULL;
	}
	
	char *uids = MSG_IMAPFolderInfoMail::AllocateImapUidString(matchingFlagKeys);
	if (uids && (m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)) 
	{
		char *url = CreateImapSetMessageFlagsUrl(((MSG_IMAPFolderInfoMail *) m_currentFolder)->GetHostName(),
												((MSG_IMAPFolderInfoMail *) m_currentFolder)->GetOnlineName(),
												((MSG_IMAPFolderInfoMail *) m_currentFolder)->GetOnlineHierarchySeparator(),
												uids, matchingFlags, TRUE);
		if (url)
		{
			XP_Bool alreadyRunningQueue;
			MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);

			if (queue)
			{
				// should we insert this at 0, or add? I think we want to run offline events
				// before any new events...but this is just a lite select
				queue->AddUrl(url, OfflineOpExitFunction);
				if (!alreadyRunningQueue)
					queue->GetNextUrl();	
			}
			XP_FREE(url);
		}
	}
	FREEIF(uids);
}

/* static */ void
OfflineImapGoOnlineState::PostAppendMsgExitFunction(URL_Struct *URL_s, int
													status, MWContext
													*window_id)
{
	AppendMsgOfflineImapState *appendMsgState = 
		(AppendMsgOfflineImapState *) URL_s->fe_data;
	// Append Msg specific 
	if (status >= 0)
	{
		appendMsgState->RemoveMsgFile();
		appendMsgState->RemoveHdrFromDB();
	}
	URL_s->fe_data = 0;
	delete appendMsgState;
	
	OfflineOpExitFunction(URL_s, status, window_id);
}

void
OfflineImapGoOnlineState::ProcessAppendMsgOperation(DBOfflineImapOperation
													*currentOp, int32 opType)
{
	MailMessageHdr *mailHdr =
		m_currentDB->GetMailHdrForKey(currentOp->GetMessageKey()); 
	if (mailHdr)
	{
		char *msg_file_name = WH_TempName (xpFileToPost, "nsmail");
		if (msg_file_name)
		{
			XP_File msg_file = XP_FileOpen(msg_file_name, xpFileToPost,
										   XP_FILE_WRITE_BIN);
			if (msg_file)
			{
				mailHdr->WriteOfflineMessage(msg_file, m_currentDB->GetDB());
				XP_FileClose(msg_file);
				XPStringObj moveDestination;
				currentOp->GetMoveDestination(m_currentDB->GetDB(), moveDestination);
				MSG_IMAPFolderInfoMail *currentIMAPFolder = m_currentFolder->GetIMAPFolderInfoMail();

				MSG_IMAPFolderInfoMail *mailFolderInfo = currentIMAPFolder
					? m_workerPane->GetMaster()->FindImapMailFolder(currentIMAPFolder->GetHostName(), moveDestination, NULL, FALSE)
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
						url->post_data_is_file = TRUE;
						url->method = URL_POST_METHOD;
						url->fe_data = (void *) new
							AppendMsgOfflineImapState(
								mailFolderInfo,
								currentOp->GetMessageKey(), msg_file_name);
						url->internal_url = TRUE;
						url->msg_pane = m_workerPane;
						m_workerPane->GetContext()->imapURLPane = m_workerPane;
						MSG_UrlQueue::AddUrlToPane (url,
													PostAppendMsgExitFunction,
													m_workerPane, TRUE);
						currentOp->ClearAppendMsgOperation(m_currentDB->GetDB(), opType);
					}
					XP_FREEIF(urlString);
				}
			}
			XP_FREEIF(msg_file_name);
		}
		delete mailHdr;
	}
	delete currentOp;
}


void OfflineImapGoOnlineState::ProcessMoveOperation(DBOfflineImapOperation *currentOp)
{
	IDArray *matchingFlagKeys = new IDArray ;
	int currentKeyIndex = m_KeyIndex;
	XPStringObj moveDestination;
	currentOp->GetMoveDestination(m_currentDB->GetDB(), moveDestination);
	XP_Bool moveMatches = TRUE;
	
	do {	// loop for all messsages with the same destination
			if (moveMatches)
			{
				matchingFlagKeys->Add(currentOp->GetMessageKey());
				currentOp->ClearMoveOperation(m_currentDB->GetDB());
			}
			delete currentOp;
			currentOp = NULL;
			
			if (++currentKeyIndex < m_CurrentKeys.GetSize())
			{
				XPStringObj nextDestination;
				currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[currentKeyIndex], FALSE);
				if (currentOp && (currentOp->GetOperationFlags() & kMsgMoved) )
				{
					currentOp->GetMoveDestination(m_currentDB->GetDB(), nextDestination);
					moveMatches = XP_STRCMP(moveDestination, nextDestination) == 0;
				}
				else 
					moveMatches = FALSE;
			}
	} while (currentOp);
	
	XP_Bool alreadyRunningQueue;
	MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);

	MSG_IMAPFolderInfoMail *currentIMAPFolder = m_currentFolder->GetIMAPFolderInfoMail();
	MSG_IMAPFolderInfoMail *destFolder = (currentIMAPFolder) 
		? m_workerPane->GetMaster()->FindImapMailFolder(currentIMAPFolder->GetHostName(), moveDestination, NULL, FALSE)
		: 0;
	m_currentFolder->StartAsyncCopyMessagesInto( destFolder,
												 m_workerPane, m_currentDB, matchingFlagKeys, matchingFlagKeys->GetSize(),
												 m_workerPane->GetContext(), queue, TRUE);
	if (!alreadyRunningQueue)
		queue->GetNextUrl();	
					
}


void OfflineImapGoOnlineState::ProcessCopyOperation(DBOfflineImapOperation *currentOp)
{
	IDArray *matchingFlagKeys = new IDArray ;
	int currentKeyIndex = m_KeyIndex;
	XPStringObj copyDestination;
	currentOp->GetIndexedCopyDestination(m_currentDB->GetDB(), 0,copyDestination);
	XP_Bool copyMatches = TRUE;
	
	do {	// loop for all messsages with the same destination
			if (copyMatches)
			{
				matchingFlagKeys->Add(currentOp->GetMessageKey());
				currentOp->ClearFirstCopyOperation(m_currentDB->GetDB());
			}
			delete currentOp;
			currentOp = NULL;
			
			if (++currentKeyIndex < m_CurrentKeys.GetSize())
			{
				XPStringObj nextDestination;
				currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[currentKeyIndex], FALSE);
				if (currentOp && (currentOp->GetOperationFlags() & kMsgCopy) )
				{
					currentOp->GetIndexedCopyDestination(m_currentDB->GetDB(), 0,copyDestination);
					copyMatches = XP_STRCMP(copyDestination, nextDestination) == 0;
				}
				else
					copyMatches = FALSE;
			}
	} while (currentOp);
	
	XP_Bool alreadyRunningQueue;
	MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);

	MSG_IMAPFolderInfoMail *currentIMAPFolder = m_currentFolder->GetIMAPFolderInfoMail();
	MSG_IMAPFolderInfoMail *destFolder = (currentIMAPFolder) 
		? m_workerPane->GetMaster()->FindImapMailFolder(currentIMAPFolder->GetHostName(), copyDestination, NULL, FALSE)
		: 0;

	m_currentFolder->StartAsyncCopyMessagesInto(destFolder, m_workerPane, m_currentDB, matchingFlagKeys, matchingFlagKeys->GetSize(),
												 m_workerPane->GetContext(), queue, FALSE);
	if (!alreadyRunningQueue)
		queue->GetNextUrl();	
}

void OfflineImapGoOnlineState::ProcessEmptyTrash(DBOfflineImapOperation *currentOp)
{
	XP_Bool alreadyRunningQueue;
	MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);

	delete currentOp;
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

		m_currentDB = NULL;	// empty trash deletes the database?
	}
}

// returns TRUE if we found a folder to create, FALSE if we're done creating folders.
XP_Bool OfflineImapGoOnlineState::CreateOfflineFolders()
{
	while (m_currentFolder)
	{
		int32 prefFlags = m_currentFolder->GetFolderPrefFlags();
		XP_Bool offlineCreate = prefFlags & MSG_FOLDER_PREF_CREATED_OFFLINE;
		if (offlineCreate)
		{
			if (CreateOfflineFolder(m_currentFolder))
				return TRUE;
		}
		AdvanceToNextFolder();
	}
	return FALSE;
}

XP_Bool OfflineImapGoOnlineState::CreateOfflineFolder(MSG_FolderInfo *folder)
{
	MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
	char *url = CreateImapMailboxCreateUrl(imapFolder->GetHostName(), imapFolder->GetOnlineName(), imapFolder->GetOnlineHierarchySeparator());
	if (url)
	{
		XP_Bool alreadyRunningQueue;
		MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);
		if (queue)
		{
			// should we insert this at 0, or add? I think we want to run offline events
			// before any new events...but this is just a lite select
			queue->AddUrl(url, OfflineOpExitFunction);
			if (!alreadyRunningQueue)
				queue->GetNextUrl();	
			return TRUE;	// this is asynch, we have to return and be called again by the OfflineOpExitFunction
		}
	}
	return FALSE;
}

// Playing back offline operations is one giant state machine that runs through ProcessNextOperation.
// The first state is creating online any folders created offline (we do this first, so we can play back
// any operations in them in the next pass)

void OfflineImapGoOnlineState::ProcessNextOperation()
{
	char *url;
	// find a folder that needs to process operations
	MSG_FolderInfo	*deletedAllOfflineEventsInFolder = NULL;

	// if we haven't created offline folders, and we're updating all folders,
	// first, find offline folders to create.
	if (!m_createdOfflineFolders)
	{
		if (m_singleFolderToUpdate)
		{
			int32 prefFlags = m_singleFolderToUpdate->GetFolderPrefFlags();
			XP_Bool offlineCreate = prefFlags & MSG_FOLDER_PREF_CREATED_OFFLINE;

			m_createdOfflineFolders = TRUE;
			if (offlineCreate && CreateOfflineFolder(m_singleFolderToUpdate))
				return;
		}
		else
		{
			if (CreateOfflineFolders())
				return;

			delete m_folderIterator;
			m_folderIterator = new MSG_FolderIterator(m_workerPane->GetMaster()->GetImapMailFolderTree());
			m_currentFolder = (MSG_FolderInfoMail *) m_folderIterator->First();
			if (m_currentFolder && (m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAP_SERVER))
				AdvanceToNextFolder();
		}
		m_createdOfflineFolders = TRUE;
	}
	while (m_currentFolder && !m_currentDB)
	{
		int32 prefFlags = m_currentFolder->GetFolderPrefFlags();
		// need to check if folder has offline events, or is configured for offline
		if (prefFlags & (MSG_FOLDER_PREF_OFFLINEEVENTS | MSG_FOLDER_PREF_OFFLINE))
		{
			if (m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)
			{
				XP_Bool wasCreated=FALSE;
				ImapMailDB::Open(m_currentFolder->GetPathname(), FALSE, &m_currentDB, m_currentFolder->GetMaster(), &wasCreated);
			}
			else
				MailDB::Open (m_currentFolder->GetPathname(), FALSE, &m_currentDB, FALSE);
		}
		if (m_currentDB)
		{
			FE_Progress(m_workerPane->GetContext(), m_currentFolder->GetName());
			m_CurrentKeys.RemoveAll();
			m_KeyIndex = 0;
			if ((m_currentDB->ListAllOfflineOpIds(m_CurrentKeys) != 0) || !m_CurrentKeys.GetSize())
			{
				m_currentDB->Close();
				m_currentDB = NULL;
			}
			else
			{
				// trash any ghost msgs
				XP_Bool deletedGhostMsgs = FALSE;
				for (int fakeIndex=0; fakeIndex < m_CurrentKeys.GetSize(); fakeIndex++)
				{
					DBOfflineImapOperation *currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[fakeIndex], FALSE);
					if (currentOp && (currentOp->GetOperationFlags() & kMoveResult))
					{
						m_currentDB->DeleteOfflineOp(currentOp->GetMessageKey());
						deletedGhostMsgs = TRUE;
						
						MailMessageHdr *mailHdr = m_currentDB->GetMailHdrForKey(currentOp->GetMessageKey());
						if (mailHdr)
						{
							m_currentDB->DeleteMessage(mailHdr->GetMessageKey(), NULL, FALSE);
							delete mailHdr;
						}
						delete currentOp;
					}
				}
				
				if (deletedGhostMsgs)
					m_currentFolder->SummaryChanged();
				
				m_CurrentKeys.RemoveAll();
				if ( (m_currentDB->ListAllOfflineOpIds(m_CurrentKeys) != 0) || !m_CurrentKeys.GetSize() )
				{
					m_currentDB->Close();
					m_currentDB = NULL;
					if (deletedGhostMsgs)
						deletedAllOfflineEventsInFolder = m_currentFolder;
				}
				else if (m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)
				{
					MSG_IMAPFolderInfoMail *imapFolder = m_currentFolder->GetIMAPFolderInfoMail();
//					if (imapFolder->GetHasOfflineEvents())
//						XP_ASSERT(FALSE);

					if (!m_pseudoOffline)	// if pseudo offline, falls through to playing ops back.
					{
						// there are operations to playback so check uid validity
						SetCurrentUIDValidity(0);	// force initial invalid state
						url = CreateImapMailboxLITESelectUrl(imapFolder->GetHostName(),
																   imapFolder->GetOnlineName(),
																   imapFolder->GetOnlineHierarchySeparator());
						if (url)
						{
							XP_Bool alreadyRunningQueue;
							MSG_UrlQueue *queue = GetUrlQueue(&alreadyRunningQueue);
							if (queue)
							{
								// should we insert this at 0, or add? I think we want to run offline events
								// before any new events...but this is just a lite select
								queue->AddUrl(url, OfflineOpExitFunction);
								if (!alreadyRunningQueue)
									queue->GetNextUrl();	
								return;	// this is asynch, we have to return as be called again by the OfflineOpExitFunction
							}
						}
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
				m_currentFolder = NULL;	// force update of this folder now.
		}
			
	}
	
	// do the current operation
	if (m_currentDB)
	{	
		XP_Bool currentFolderFinished = FALSE;
														// user canceled the lite select! if GetCurrentUIDValidity() == 0
		if ((m_KeyIndex < m_CurrentKeys.GetSize()) && (m_pseudoOffline || (GetCurrentUIDValidity() != 0) || !(m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)) )
		{
			XP_Bool uidvalidityChanged = (!m_pseudoOffline && m_currentFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX) && (GetCurrentUIDValidity() != m_currentDB->m_dbFolderInfo->GetImapUidValidity());
			DBOfflineImapOperation *currentOp = NULL;
			if (uidvalidityChanged)
				DeleteAllOfflineOpsForCurrentDB();
			else
				currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], FALSE);
			
			if (currentOp)
			{
				// loop until we find the next db record that matches the current playback operation
				while (currentOp && !(currentOp->GetOperationFlags() & mCurrentPlaybackOpType))
				{
					delete currentOp;
					currentOp = NULL;
					if (++m_KeyIndex < m_CurrentKeys.GetSize())
						currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], FALSE);
				}
				
				// if we did not find a db record that matches the current playback operation,
				// then move to the next playback operation and recurse.  
				if (!currentOp)
				{
					// we are done with the current type
					if (mCurrentPlaybackOpType == kFlagsChanged)
					{
						mCurrentPlaybackOpType = kMsgCopy;
						// recurse to deal with next type of operation
						m_KeyIndex = 0;
						ProcessNextOperation();
					}
					else if (mCurrentPlaybackOpType == kMsgCopy)
					{
						mCurrentPlaybackOpType = kMsgMoved;
						// recurse to deal with next type of operation
						m_KeyIndex = 0;
						ProcessNextOperation();
					}
					else if (mCurrentPlaybackOpType == kMsgMoved)
					{
						mCurrentPlaybackOpType = kAppendDraft;
						// recurse to deal with next type of operation
						m_KeyIndex = 0;
						ProcessNextOperation();
					}
					else if (mCurrentPlaybackOpType == kAppendDraft)
					{
						mCurrentPlaybackOpType = kAppendTemplate;
						// recurse to deal with next type of operation
						m_KeyIndex = 0;
						ProcessNextOperation();
					}
					else if (mCurrentPlaybackOpType == kAppendTemplate)
					{
						mCurrentPlaybackOpType = kDeleteAllMsgs;
						m_KeyIndex = 0;
						ProcessNextOperation();
					}
					else
					{
						DeleteAllOfflineOpsForCurrentDB();
						currentFolderFinished = TRUE;
					}
					
				}
				else
				{
					if (mCurrentPlaybackOpType == kFlagsChanged)
						ProcessFlagOperation(currentOp);
					else if (mCurrentPlaybackOpType == kMsgCopy)
						ProcessCopyOperation(currentOp);
					else if (mCurrentPlaybackOpType == kMsgMoved)
						ProcessMoveOperation(currentOp);
					else if (mCurrentPlaybackOpType == kAppendDraft)
						ProcessAppendMsgOperation(currentOp, kAppendDraft);
					else if (mCurrentPlaybackOpType == kAppendTemplate)
						ProcessAppendMsgOperation(currentOp, kAppendTemplate);
					else if (mCurrentPlaybackOpType == kDeleteAllMsgs)
						ProcessEmptyTrash(currentOp);
					else
						XP_ASSERT(FALSE);
					// currentOp was RemoveReferencered by one of the Process functions
					// so do not reference it again!
					currentOp = NULL;
				}
			}
			else
				currentFolderFinished = TRUE;
		}
		else
			currentFolderFinished = TRUE;
			
		if (currentFolderFinished)
		{
			m_currentDB->Close();
			m_currentDB = NULL;
			if (!m_singleFolderToUpdate)
			{
				AdvanceToNextFolder();
				ProcessNextOperation();
				return;
			}
			else
				m_currentFolder = NULL;
		}
	}
	
	if (!m_currentFolder && !m_mailboxupdatesStarted)
	{
		m_mailboxupdatesStarted = TRUE;
		FE_Progress(m_workerPane->GetContext(), "");
		
		// if we are updating more than one folder then we need the iterator
		MSG_FolderIterator *updateFolderIterator = m_singleFolderToUpdate ? (MSG_FolderIterator *)NULL : new MSG_FolderIterator(m_workerPane->GetMaster()->GetImapMailFolderTree());
		MSG_UrlQueue *queue = NULL;
		
		if ((updateFolderIterator || m_singleFolderToUpdate))
		{
			if (updateFolderIterator)
			{
				// this means that we are updating all of the folders.  Update the INBOX first so the updates on the remaining
				// folders pickup the results of any filter moves.
				MSG_FolderInfo *inboxFolder;
				if (!m_pseudoOffline && m_workerPane->GetMaster()->GetImapMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &inboxFolder, 1) )
					((MSG_IMAPFolderInfoMail *) inboxFolder)->StartUpdateOfNewlySelectedFolder(m_workerPane, FALSE, queue, NULL, FALSE, FALSE);
			}
			
			
			// we are done playing commands back, now queue up the sync with each imap folder
			MSG_FolderInfo* folder = m_singleFolderToUpdate ? m_singleFolderToUpdate : updateFolderIterator->First();
			while (folder)
			{            
				XP_Bool loadingFolder = m_workerPane->GetLoadingImapFolder() == folder;
				if ((folder->GetType() == FOLDER_IMAPMAIL) && (deletedAllOfflineEventsInFolder == folder || (folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_OFFLINE)
					|| loadingFolder) 
					&& !(folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT) )
				{
					MSG_IMAPFolderInfoMail *imapMail = (MSG_IMAPFolderInfoMail *) folder;
					XP_Bool lastChance = ((deletedAllOfflineEventsInFolder == folder) && m_singleFolderToUpdate) || loadingFolder;
					// if deletedAllOfflineEventsInFolder == folder and we're only updating one folder, then we need to update newly selected folder
					// I think this also means that we're really opening the folder...so we tell StartUpdate that we're loading a folder.
					if (!updateFolderIterator || !(imapMail->GetFlags() & MSG_FOLDER_FLAG_INBOX))		// avoid queueing the inbox twice
						imapMail->StartUpdateOfNewlySelectedFolder(m_workerPane, lastChance, queue, NULL, FALSE, FALSE);
				}
				folder= m_singleFolderToUpdate ? (MSG_FolderInfo *)NULL : updateFolderIterator->Next();
			}
			
			MSG_UrlQueue::AddUrlToPane(kImapOnOffSynchCompleteURL, OffOnlineSynchExitFunction, m_workerPane);
		}
	}
}


void OfflineImapGoOnlineState::DeleteAllOfflineOpsForCurrentDB()
{
	m_KeyIndex = 0;
	DBOfflineImapOperation *currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], FALSE);
	while (currentOp)
	{
		XP_ASSERT(currentOp->GetOperationFlags() == 0);
		// delete any ops that have already played back
		m_currentDB->DeleteOfflineOp(currentOp->GetMessageKey());
		delete currentOp;
		currentOp = NULL;
		
		if (++m_KeyIndex < m_CurrentKeys.GetSize())
			currentOp = m_currentDB->GetOfflineOpForKey(m_CurrentKeys[m_KeyIndex], FALSE);
	}
	MSG_FolderInfo *folderInfo = m_currentDB->GetFolderInfo();
	// turn off MSG_FOLDER_PREF_OFFLINEEVENTS
	if (folderInfo)
	{
		folderInfo->SetFolderPrefFlags(folderInfo->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_OFFLINEEVENTS);
		if (folderInfo->GetType() == FOLDER_IMAPMAIL)
		{
			MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) folderInfo;
			imapFolder->SetHasOfflineEvents(FALSE);
			// where should we clear this flag? At the end of the process events url?
		}
	}
}


OfflineImapState::OfflineImapState(MSG_IMAPFolderInfoMail *folder)
{
	XP_ASSERT(folder);
	
	m_maildb = NULL;
	m_msgHdr = NULL;
	SetFolderInfo(folder);
}

void OfflineImapState::SetFolderInfo(MSG_IMAPFolderInfoMail *folder)
{
	
	XP_Bool wasCreated=FALSE;
	if (m_folderInfo != folder)
	{
		if (m_maildb)
			m_maildb->Close();
		ImapMailDB::Open(folder->GetPathname(), TRUE, &m_maildb, folder->GetMaster(), &wasCreated);
	}
	m_folderInfo = folder;
}

OfflineImapState::~OfflineImapState()
{
	delete m_msgHdr;
	if (m_maildb)
		m_maildb->Close();
}
	
DownloadOfflineImapState::DownloadOfflineImapState(MSG_IMAPFolderInfoMail *folder, NET_StreamClass *displayStream) : 
	OfflineImapState(folder), m_displayStream(displayStream)
{
	m_offlineWriteFailure = FALSE;
	m_dbWriteDocument = NULL;
	m_deleteOnComplete = TRUE;
}

DownloadOfflineImapState::~DownloadOfflineImapState()
{
	MSG_OfflineMsgDocumentHandle_Destroy(m_dbWriteDocument);
}


//static 
void DownloadOfflineImapState::CreateOfflineImapStream(NET_StreamClass *theStream, MSG_IMAPFolderInfoMail *folder, NET_StreamClass *displayStream, DownloadOfflineImapState *closureData)
{
	XP_Bool deleteOnComplete = (closureData == NULL);
	if (!closureData)
	{
		closureData = new DownloadOfflineImapState(folder, displayStream);
	}
	else
	{
		closureData->m_displayStream = displayStream;	// memory leak?
		closureData->SetFolderInfo(folder);
	}
	theStream->data_object 		= closureData;
	theStream->is_write_ready	= DownloadOfflineImapState::OfflineImapStreamWriteReady;
    theStream->put_block 		= DownloadOfflineImapState::OfflineImapStreamWrite;
    theStream->complete  		= DownloadOfflineImapState::OfflineImapStreamComplete;
    theStream->abort     		= DownloadOfflineImapState::OfflineImapStreamAbort;
}

//static 
void DownloadOfflineImapState::OfflineImapStreamComplete(NET_StreamClass *stream)
{
	DownloadOfflineImapState *state = (DownloadOfflineImapState *) stream->data_object;	
	if (state)
	{
		state->CompleteStream();
		if (state->m_dbWriteDocument)
			MSG_OfflineMsgDocumentHandle_Complete(state->m_dbWriteDocument);
		if (state->m_deleteOnComplete)
			delete state;
	}
}

//static 
void DownloadOfflineImapState::OfflineImapStreamAbort (NET_StreamClass *stream, int status)
{
	DownloadOfflineImapState *state = (DownloadOfflineImapState *) stream->data_object;	
	if (state)
	{
		state->AbortStream(status);
		delete state;
	}
}

//static 
int DownloadOfflineImapState::OfflineImapStreamWrite (NET_StreamClass *stream, const char *block, int32 messageKey)
{
	DownloadOfflineImapState *state = (DownloadOfflineImapState *) stream->data_object;	
	if (state)
		return state->WriteStream(block, messageKey);
	else
		return XP_STRLEN(block);
}
	
int DownloadOfflineImapState::WriteStream(const char *block, uint32 messageKey)
{
	int returnLength = 0;
	int blockLength = XP_STRLEN(block);
	if (m_maildb)
	{
		if (!m_msgHdr || (m_msgHdr->GetMessageKey() != messageKey))
		{
			if (m_msgHdr)
			{
				// this should not happen but be paranoid
				m_msgHdr->PurgeOfflineMessage(m_maildb->GetDB());
				delete m_msgHdr;
			}
			m_msgHdr = m_maildb->GetMailHdrForKey(messageKey);
			if (!m_dbWriteDocument)
				m_dbWriteDocument = MSG_OfflineMsgDocumentHandle_Create(m_maildb->GetDB(), m_msgHdr->GetHandle());
			else
				MSG_OfflineMsgDocumentHandle_SetMsgHeaderHandle(m_dbWriteDocument, m_msgHdr->GetHandle(), m_maildb->GetDB());
		}
		
		
		if (m_msgHdr && m_dbWriteDocument && !m_offlineWriteFailure)
		{
//			int32 bytesAdded = m_msgHdr->AddToOfflineMessage(block, blockLength, m_maildb->GetDB());
			int32 bytesAdded = MSG_OfflineMsgDocumentHandle_AddToOfflineMessage(m_dbWriteDocument, block, blockLength);
			m_offlineWriteFailure = bytesAdded != blockLength;
		}
	}
	
	if (m_maildb)
		returnLength += blockLength;
		
	
	if (m_displayStream)
        (*m_displayStream->put_block) (m_displayStream,
                                    	block,
                                    	XP_STRLEN(block));
	
	return returnLength;
}

void DownloadOfflineImapState::CompleteStream() 
{
	if (m_maildb && m_msgHdr)
	{
		m_maildb->MarkOffline(m_msgHdr->GetMessageKey(), TRUE, NULL);
		delete m_msgHdr;
		m_msgHdr = NULL;
	}
	
	if ( m_displayStream)
		(*m_displayStream->complete) (m_displayStream);

}

void DownloadOfflineImapState::AbortStream(int /*status*/)
{
	if (m_maildb && m_msgHdr)
	{
		m_msgHdr->PurgeOfflineMessage(m_maildb->GetDB());
		delete m_msgHdr;
		m_msgHdr = NULL;
	}
	if ( m_displayStream)
		(*m_displayStream->abort) (m_displayStream, -1);
}


DisplayOfflineImapState::DisplayOfflineImapState(MSG_IMAPFolderInfoMail *folder, MessageKey key) : OfflineImapState(folder)
{
	m_bytesDisplayedSoFar = 0;
	m_wasUnread = FALSE;
	
	if (m_maildb)
	{
		m_msgHdr = m_maildb->GetMailHdrForKey(key);
		if (m_msgHdr)
			m_wasUnread = !((m_msgHdr->GetFlags() & kIsRead) != 0);
	}
}  

DisplayOfflineImapState::~DisplayOfflineImapState()
{
}

uint32 DisplayOfflineImapState::GetMsgSize()
{
	uint32 size = 0;
	if (m_msgHdr)
		size = m_msgHdr->GetOfflineMessageLength(m_maildb->GetDB());
	return size;
}

int DisplayOfflineImapState::ProcessDisplay(char *socketBuffer, uint32 read_size)
{
	uint32 bytesDisplayed = 0;
	if (m_msgHdr)
	{
		int32 offlineLength = m_msgHdr->GetOfflineMessageLength(m_maildb->GetDB());
		
		if (0 == offlineLength)
		{
			if (0 == m_bytesDisplayedSoFar)
			{
				// annoy the user about going online
				const char *htmlAnnoyance = XP_GetString(MK_MSG_HTML_IMAP_NO_CACHED_BODY);
				bytesDisplayed = XP_STRLEN( htmlAnnoyance ) < read_size ? XP_STRLEN( htmlAnnoyance ) + 1 : read_size;
				XP_MEMCPY(socketBuffer, htmlAnnoyance, bytesDisplayed);
				m_bytesDisplayedSoFar = bytesDisplayed;
			}
			// else bytesDisplayed == 0, ProcessDisplay is not called again
		}
		else
		{
			uint32 bytesLeft      = offlineLength - m_bytesDisplayedSoFar;
			uint32 bytesToDisplay = (read_size <= bytesLeft) ? read_size : bytesLeft;
			
			if (bytesToDisplay)
			{
				bytesDisplayed += m_msgHdr->ReadFromOfflineMessage(socketBuffer, bytesToDisplay, m_bytesDisplayedSoFar, m_maildb->GetDB());
				m_bytesDisplayedSoFar += bytesDisplayed;
			}
			else if (NET_IsOffline() && m_wasUnread)
			{
				// we are done displaying this message.  Save an operation to mark it read
				IDArray readIds;
				readIds.Add(m_msgHdr->GetMessageKey());
				m_folderInfo->StoreImapFlags(NULL, kImapMsgSeenFlag, TRUE, readIds);
			}
		}
	}
	
	return bytesDisplayed;
}

AppendMsgOfflineImapState::AppendMsgOfflineImapState(MSG_IMAPFolderInfoMail
													 *folder, MessageKey key,
													 const char *msg_file_name)
	: OfflineImapState(folder)
{
	m_msg_file_name = XP_STRDUP(msg_file_name);
	if (m_maildb)
	{
		m_msgHdr = m_maildb->GetMailHdrForKey(key);
	}
}  

AppendMsgOfflineImapState::~AppendMsgOfflineImapState()
{
	XP_FREEIF(m_msg_file_name);
}

void
AppendMsgOfflineImapState::RemoveHdrFromDB()
{
	if (m_maildb && m_msgHdr)
	{
		MessageKey doomedKey = m_msgHdr->GetMessageKey();
		delete m_msgHdr;
		m_msgHdr = 0;
		m_maildb->DeleteMessage(doomedKey);
	}
}

void
AppendMsgOfflineImapState::RemoveMsgFile()
{
	if (m_msg_file_name)
	{
		XP_FileRemove(m_msg_file_name, xpFileToPost);
		XP_FREEIF(m_msg_file_name);
	}
}

/*
	member functions for offlinedb records
*/

DBOfflineImapOperation::DBOfflineImapOperation()
{
	m_offlineIMAPOperationHandle = NULL;
}

DBOfflineImapOperation::~DBOfflineImapOperation()
{
	if (m_offlineIMAPOperationHandle)
	   MSG_OfflineIMAPOperationHandle_RemoveReference(m_offlineIMAPOperationHandle);
}

void DBOfflineImapOperation::SetMessageKey(MessageKey messageKey)
{
	MSG_OfflineIMAPOperationHandle_SetMessageKey(GetHandle(), m_dbHandle, messageKey);	
}

	// the flags we start with
void DBOfflineImapOperation::SetInitialImapFlags(imapMessageFlagsType flags) 
{
	MSG_OfflineIMAPOperationHandle_SetInitialImapFlags(GetHandle(),	flags, m_dbHandle);
}

void DBOfflineImapOperation::SetImapFlagOperation(imapMessageFlagsType flags)
{
	MSG_OfflineIMAPOperationHandle_SetImapFlagOperation(GetHandle(), flags, m_dbHandle);
}

void DBOfflineImapOperation::ClearImapFlagOperation()
{
	MSG_OfflineIMAPOperationHandle_ClearImapFlagOperation(GetHandle(), m_dbHandle);
}

uint32	DBOfflineImapOperation::GetOperationFlags() 
{ 
	return MSG_OfflineIMAPOperationHandle_GetOperationFlags(GetHandle(), m_dbHandle); 
}

imapMessageFlagsType DBOfflineImapOperation::GetNewMessageFlags() 
{
	return MSG_OfflineIMAPOperationHandle_GetNewMessageFlags(GetHandle(), m_dbHandle); 
}

void 
DBOfflineImapOperation::SetAppendMsgOperation(const char *destinationBox, 
											   int32 opType)
{
	MSG_OfflineIMAPOperationHandle_SetAppendMsgOperation(GetHandle(), m_dbHandle, destinationBox, opType);
}

void DBOfflineImapOperation::ClearAppendMsgOperation(MSG_DBHandle dbHandle, int32 opType)
{
	MSG_OfflineIMAPOperationHandle_ClearAppendMsgOperation(GetHandle(), dbHandle, opType);
}

void DBOfflineImapOperation::SetMessageMoveOperation(const char *destinationBox)
{
	MSG_OfflineIMAPOperationHandle_SetMessageMoveOperation(GetHandle(), m_dbHandle, destinationBox);
}

void DBOfflineImapOperation::ClearMoveOperation(MSG_DBHandle dbHandle)
{
	MSG_OfflineIMAPOperationHandle_ClearMoveOperation(GetHandle(), dbHandle);
}

void DBOfflineImapOperation::AddMessageCopyOperation(const char *destinationBox)
{
	MSG_OfflineIMAPOperationHandle_AddMessageCopyOperation(GetHandle(), m_dbHandle, destinationBox);
}

void DBOfflineImapOperation::ClearFirstCopyOperation(MSG_DBHandle dbHandle)
{
	MSG_OfflineIMAPOperationHandle_ClearFirstCopyOperation(GetHandle(), m_dbHandle);
}

void DBOfflineImapOperation::SetDeleteAllMsgs()
{
	MSG_OfflineIMAPOperationHandle_SetDeleteAllMsgs(GetHandle(), m_dbHandle);
}

void DBOfflineImapOperation::ClearDeleteAllMsgs()
{
	MSG_OfflineIMAPOperationHandle_ClearDeleteAllMsgs(GetHandle(), m_dbHandle);
}

void DBOfflineImapOperation::SetSourceMailbox(const char *sourceMailbox, MessageId sourceKey)
{
	MSG_OfflineIMAPOperationHandle_SetSourceMailbox(GetHandle(), m_dbHandle, sourceMailbox, sourceKey);
}


// This is the key used in the database, which is almost always the same
// as the m_messageKey, except for the first message in a mailbox,
// which has a m_messageKey of 0, but a non-zero ID in the database.
MessageKey DBOfflineImapOperation::GetMessageKey()
{
	return MSG_OfflineIMAPOperationHandle_GetMessageKey(GetHandle(), m_dbHandle);
}


XP_Bool DBOfflineImapOperation::GetMoveDestination(MSG_DBHandle dbHandle, XPStringObj &boxName)
{
	char *boxNameStr;
	XP_Bool ret = MSG_OfflineIMAPOperationHandle_GetMoveDestination(GetHandle(), dbHandle, &boxNameStr);
	boxName.SetStrPtr(boxNameStr);
	return ret;
}


uint32 DBOfflineImapOperation::GetNumberOfCopyOps()
{
	return MSG_OfflineIMAPOperationHandle_GetNumberOfCopyOps(GetHandle(), m_dbHandle);
}

XP_Bool DBOfflineImapOperation::GetIndexedCopyDestination(MSG_DBHandle dbHandle, uint32 index, XPStringObj &boxName)
{
	char *boxNameStr;
	XP_Bool ret = MSG_OfflineIMAPOperationHandle_GetIndexedCopyDestination(GetHandle(), dbHandle, index, &boxNameStr);
	if (ret)
		boxName.SetStrPtr(boxNameStr);

	return ret;
}

XP_Bool DBOfflineImapOperation::GetSourceInfo(XPStringObj &sourceBoxName, MessageId &sourceKey)
{
	sourceKey = MSG_OfflineIMAPOperationHandle_GetSourceMessageKey(GetHandle(), m_dbHandle);
	return GetMoveDestination(m_dbHandle, sourceBoxName);
}


