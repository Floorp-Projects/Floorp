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

#include "rosetta.h"
#include "msg.h"
#include "msgcom.h"
#include "errcode.h"

#include "msgtpane.h"
#include "msgfpane.h"
#include "msgmpane.h"
#include "msgfinfo.h"
#include "msgdbvw.h"
#include "msgmast.h"
#include "msgprefs.h"
#include "listngst.h"
#include "maildb.h"
#include "newsdb.h"
#include "prsembst.h"
#include "msgundmg.h"
#include "msgundac.h"
#include "msgurlq.h"
#include "msgcflds.h"
#include "msgcpane.h"
#include "addrbook.h"
#include "grpinfo.h"
#include "thrdbvw.h"
#include "imapoff.h"
#include "prefapi.h"
#include "xpgetstr.h"
#include "xp_qsort.h"
#include "msgimap.h"

extern "C"
{
	extern int MK_MSG_BY_MESSAGE_NB;
	extern int MK_MSG_BY_DATE;
	extern int MK_MSG_BY_SENDER;
	extern int MK_MSG_BY_SUBJECT;
	extern int MK_MSG_BY_PRIORITY;
	extern int MK_MSG_BY_SIZE;
	extern int MK_MSG_BY_UNREAD;
	extern int MK_MSG_BY_FLAG;
	extern int MK_MSG_SORT_BACKWARD;
	extern int MK_MSG_BY_STATUS;
	extern int MK_MSG_COMPRESS_FOLDER;
	extern int MK_MSG_THREAD_MESSAGES;
	extern int MK_MSG_KBYTES_WASTED;
	extern int MK_MSG_DELETE_SEL_MSGS;
	extern int MK_MSG_FOLDER_UNREADABLE;
	extern int MK_MSG_ALIVE_THREADS;
	extern int MK_MSG_KILLED_THREADS;
	extern int MK_MSG_THREADS_WITH_NEW;
	extern int MK_MSG_WATCHED_THREADS;
	extern int MK_MSG_NEW_MESSAGES_ONLY;
	extern int MK_MSG_NEW_NEWS_MESSAGE;
	extern int MK_MSG_POST_REPLY;
	extern int MK_MSG_POST_MAIL_REPLY;
	extern int MK_MSG_REPLY;
	extern int MK_MSG_REPLY_TO_ALL;
	extern int MK_MSG_REPLY_TO_SENDER;
	extern int MK_MSG_FORWARD_QUOTED;
	extern int MK_MSG_FORWARD;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_ADD_SENDER_TO_ADDRESS_BOOK;
	extern int MK_MSG_ADD_ALL_TO_ADDRESS_BOOK;
	extern int MK_MSG_CANT_OPEN;
}

ThreadPaneListener::ThreadPaneListener(MSG_Pane *pane) : PaneListener(pane)
{
}

ThreadPaneListener::~ThreadPaneListener()
{
}
	
MSG_ThreadPane::MSG_ThreadPane(MWContext* context, MSG_Master* master)
	: m_threadPaneListener(this), MSG_LinedPane(context, master)
{
	m_imapUpgradeBeginFolder = 0;
}

MSG_ThreadPane::~MSG_ThreadPane() 
{
	ClearNewBits(FALSE);
	if (m_view != NULL)
	{
		m_view->Remove(&m_threadPaneListener);
		m_view->Close(); 
	}
	m_view = NULL;
}

void MSG_ThreadPane::ClearNewBits(XP_Bool notify)
{
	if (m_view != NULL)
	{
		if (m_view->GetDB())
			m_view->GetDB()->ClearNewList(notify);
	}
	if (m_folder)
	{
	    m_folder->ClearFlag(MSG_FOLDER_FLAG_GOT_NEW);
	    m_master->BroadcastFolderChanged(m_folder);
	}
}

MSG_PaneType MSG_ThreadPane::GetPaneType() 
{
	return MSG_THREADPANE;
}

void MSG_ThreadPane::StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num)
{
	if (m_numstack == 0)
		m_threadPaneListener.StartKeysChanging();

	MSG_LinedPane::StartingUpdate(code, where, num);
}

void MSG_ThreadPane::EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num)
{
	MSG_LinedPane::EndingUpdate(code, where, num);
	if (m_numstack == 0)
		m_threadPaneListener.EndKeysChanging();
}

ViewType MSG_ThreadPane::GetViewTypeFromCommand(MSG_CommandType command)
{
	switch (command)
	{
	case MSG_ViewNewOnly:
		return ViewOnlyNewHeaders;
	case MSG_ViewAllThreads:
		return ViewAllThreads;
	case MSG_ViewWatchedThreadsWithNew:
		return ViewWatchedThreadsWithNew;
	case MSG_ViewThreadsWithNew:
		return ViewOnlyThreadsWithNew;
	default:
		XP_ASSERT(FALSE);
		return ViewAllThreads;
	}
}

MsgERR MSG_ThreadPane::SwitchViewType(MSG_CommandType command)
{
	char	*url = NULL;
	ViewType	viewType;
	if (command == MSG_ViewKilledThreads)
	{
		viewType = m_view->GetViewType();
		MessageDB *msgDB = m_view->GetDB();
		DBFolderInfo *folderInfo = (msgDB) ? msgDB->GetDBFolderInfo() : 0;
		if (folderInfo)
		{
			XP_Bool showingIgnored = folderInfo->TestFlag(MSG_FOLDER_PREF_SHOWIGNORED);
			if (showingIgnored)
				folderInfo->AndFlags(~MSG_FOLDER_PREF_SHOWIGNORED);
			else
				folderInfo->OrFlags(MSG_FOLDER_PREF_SHOWIGNORED);
		}
	}
	else
	{
		viewType = GetViewTypeFromCommand(command);
		if (m_view && viewType == m_view->GetViewType())
			return eSUCCESS;	// already in that view.
	}
	// If we're currently building a view in the background, we want to interrupt that.
	msg_InterruptContext(GetContext(), TRUE); 

	StartingUpdate(MSG_NotifyScramble, 0, 0);

	url = m_folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);

	MessageDBView	*newView;
	MsgERR status = MessageDBView::OpenURL(url, GetMaster(),
						 viewType, &newView, TRUE);

	m_view->NotifyAnnouncerChangingView(m_view, newView);
	m_view->Remove(&m_threadPaneListener);
	m_view->Close();
	m_view = newView;

	EndingUpdate(MSG_NotifyScramble, 0, 0);

	if (status == eBuildViewInBackground)
		status = ListThreads();

	if (status == eSUCCESS && m_view != NULL)
		m_view->Add(&m_threadPaneListener); // add listener back to view
									   // this might be wrong if close
									// didn't remove it.

	XP_FREE(url);
	return status;
}

MsgERR MSG_ThreadPane::ResetView(ViewType typeOfView)
{
	StartingUpdate(MSG_NotifyAll, 0, 0);
	
	if (m_view)
	{
		m_view->Close();
	}
	
	m_view = new ThreadDBView(typeOfView);
	if (m_view != NULL)
		m_view->Add(&m_threadPaneListener); // add listener back to view
	
	EndingUpdate(MSG_NotifyAll, 0, 0);
	return 0;						   
}


typedef struct sortTableEntry
{
	SortType		sortType;
	MSG_CommandType	msgCommand;
}	SortTableEntry;

SortTableEntry	SortTable[] =
{
	{ SortByThread, MSG_SortByThread },
	{ SortByAuthor, MSG_SortBySender },
	{ SortBySubject, MSG_SortBySubject },
	{ SortByDate, MSG_SortByDate },
	{ SortById, MSG_SortByMessageNumber },
	{ SortByPriority, MSG_SortByPriority },
	{ SortByStatus, MSG_SortByStatus },
	{ SortBySize, MSG_SortBySize },
	{ SortByFlagged, MSG_SortByFlagged },
	{ SortByUnread, MSG_SortByUnread }
};

/* static */SortType MSG_ThreadPane::GetSortTypeFromCommand(MSG_CommandType command)
{
	if (command == MSG_SortByDate)
	{
		XP_Bool	sortByDateReceived = FALSE;
		PREF_GetBoolPref("mailnews.sort_by_date_received", &sortByDateReceived);
		if (sortByDateReceived)
			command = MSG_SortByMessageNumber;
	}

	for (unsigned int i = 0; i < sizeof(SortTable) / sizeof(SortTableEntry); i++)
	{
		if (SortTable[i].msgCommand == command)
			return SortTable[i].sortType;
	}
	XP_ASSERT(FALSE);
	return SortByThread;
}

MsgERR
MSG_ThreadPane::DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
					int32 numIndices)
{
	MsgERR status = 0;
	SortType	sortType;
	XP_Bool	sortAscending = TRUE;  
	XP_Bool calledFEEnd = FALSE;

	if (command != MSG_MailNew && (!m_view || !m_view->GetDB())) return FALSE;

	if (command == MSG_ForwardMessage)
	{
		int32 forwardMsgMode = 0;
		PREF_GetIntPref("mail.forward_message_mode", &forwardMsgMode);
		if (forwardMsgMode == 1)
			command = MSG_ForwardMessageQuoted;
		else if (forwardMsgMode == 2)
			command = MSG_ForwardMessageInline;
		else
			command = MSG_ForwardMessageAttachment;
	}

	FEStart();  // N.B. Don't return before FEEnd()
	switch (command) {

	case MSG_SortBySender:
	case MSG_SortByThread:
	case MSG_SortBySubject:
	case MSG_SortByDate:
	case MSG_SortByMessageNumber:
	case MSG_SortByPriority:
	case MSG_SortByStatus:
	case MSG_SortBySize:
	case MSG_SortByFlagged:
	case MSG_SortByUnread:
		sortType = GetSortTypeFromCommand(command);
		if (DisplayingRecipients() && sortType == SortByAuthor)
			sortType = SortByRecipient;
		if (m_view->GetSortType() == sortType)
			sortAscending = (m_view->GetSortOrder() != SortTypeAscending);


		StartingUpdate(MSG_NotifyScramble, 0, 0);
		status = m_view->ExternalSort(sortType, sortAscending); 
		EndingUpdate(MSG_NotifyScramble, 0, 0);
		break;

	case MSG_SortBackward:
		sortAscending = m_view->GetSortOrder() != SortTypeAscending;

		StartingUpdate(MSG_NotifyScramble, 0, 0);
		status = m_view->ExternalSort(m_view->GetSortType(), sortAscending); 
		EndingUpdate(MSG_NotifyScramble, 0, 0);
		break;

	case MSG_PostNew:
		ComposeNewsMessage(m_folder);
		break;
	case MSG_PostReply:
	case MSG_PostAndMailReply:
	case MSG_ReplyToSender:
	case MSG_ReplyToAll:
	case MSG_ForwardMessageQuoted:
		status = ComposeMessage(command, indices, numIndices);
		break;
	case MSG_ForwardMessageAttachment:
		// ###tw  FIX ME!  This only works with one message!
		status = ForwardMessages(indices, numIndices);
		break;
	case MSG_AddSender:
	case MSG_AddAll:
		if (numIndices > 1)
			XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), CompareViewIndices);

		FEEnd();
		calledFEEnd = TRUE;
		AB_SetIsImporting (FE_GetAddressBook(this), TRUE);
		MSG_Pane::ApplyCommandToIndices(command, indices, numIndices);
		AB_SetIsImporting (FE_GetAddressBook(this), FALSE);
		break;

	case MSG_CollapseAll:
		StartingUpdate(MSG_NotifyNone, 0, 0);
		status = m_view->CollapseAll();
		EndingUpdate(MSG_NotifyNone, 0, 0);
		break;
	case MSG_ExpandAll:
		StartingUpdate(MSG_NotifyNone, 0, 0);
		status = m_view->ExpandAll();
		EndingUpdate(MSG_NotifyNone, 0, 0);
		break;
	case MSG_MarkMessages:
	case MSG_UnmarkMessages:
	case MSG_MarkMessagesRead:
	case MSG_MarkMessagesUnread:
	case MSG_ToggleMessageRead:
	case MSG_DeleteMessage:
	case MSG_DeleteMessageNoTrash:
		// since the FE could have constructed the list of indices in
		// any order (e.g. order of discontiguous selection), we have to
		// sort the indices in order to find out which MSG_ViewIndex will
		// be deleted first.
		if (numIndices > 1)
			XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), CompareViewIndices);

		if (command != MSG_DeleteMessage && 
			command != MSG_DeleteMessageNoTrash)
			GetUndoManager()->AddUndoAction(
				new MarkMessageUndoAction(this, command, indices, 
										  numIndices, GetFolder()));
		FEEnd();
		calledFEEnd = TRUE;
		StartingUpdate(MSG_NotifyNone, 0, 0);
		ApplyCommandToIndices(command, indices, numIndices);
		EndingUpdate(MSG_NotifyNone, 0, 0);

		break;
	case MSG_ViewNewOnly:
	case MSG_ViewKilledThreads:
	case MSG_ViewAllThreads :
	case MSG_ViewThreadsWithNew:		/* t: Show only threads with new messages */
	case MSG_ViewWatchedThreadsWithNew: /* t: Show only watched thrds with new msgs */
		SwitchViewType(command);
		break;
	case MSG_ModerateNewsgroup:
		status = ModerateNewsgroup (GetFolder());
		break;
	case MSG_NewCategory:
		status = NewNewsgroup (GetFolder(), TRUE);
		break;
	case MSG_CompressFolder:
		{
			MSG_FolderInfoMail *mailFolder = GetFolder()->GetMailFolderInfo();
			if (mailFolder)
				status = CompressOneMailFolder(mailFolder);
			else
				XP_ASSERT(FALSE);	// MSG_CommandStatus should have failed!
			break;
		}
	default:
		status = MSG_Pane::DoCommand(command, indices, numIndices);
		break;
	}
	if (!calledFEEnd)
		FEEnd();
	return status;
}


MsgERR
MSG_ThreadPane::GetCommandStatus(MSG_CommandType command,
						   const MSG_ViewIndex* indices, int32 numindices,
						   XP_Bool *selectable_pP,
						   MSG_COMMAND_CHECK_STATE *selected_pP,
						   const char **display_stringP,
						   XP_Bool *plural_pP)
{
	// don't want to go through the null view and db check for get new mail.
	if (command == MSG_GetNewMail || command == MSG_MailNew)
		return  MSG_Pane::GetCommandStatus( command, indices, numindices,
				   selectable_pP, selected_pP, display_stringP, plural_pP);

	if (!m_view) return FALSE;

	const char *display_string = 0;
	XP_Bool plural_p = FALSE;
	// N.B. default is TRUE, so you don't need to set it in each case
	XP_Bool selectable_p = TRUE;	
	XP_Bool selected_p = FALSE;
	XP_Bool selected_used_p = FALSE;
	XP_Bool news_p = (m_folder && m_folder->IsNews());
	XP_Bool gotBody = TRUE;
	// if we're offline, and we don't have the body of a news or imap message, then
	// dis
	if (NET_IsOffline() && (news_p || (m_folder && m_folder->GetType() == FOLDER_IMAPMAIL)))
	{
		for (int32 index = 0; index < numindices; index++)
		{
			char extraFlag = 0;
			m_view->GetExtraFlag(indices[index], &extraFlag);
			if (! (extraFlag & kOffline))
			{
				gotBody = FALSE;
				break;
			}
		}
	}
	MessageDB* db = m_view->GetDB();
	if (!db) return FALSE;
	switch(command) {
	case MSG_CompressFolder:
		selectable_p = m_folder->IsMail();
		display_string = XP_GetString(MK_MSG_COMPRESS_FOLDER);
		break;
	case MSG_PostNew:
		selectable_p = news_p && m_folder->AllowsPosting();
		display_string = XP_GetString(MK_MSG_NEW_NEWS_MESSAGE);
		break;
	case MSG_PostReply:
		display_string = XP_GetString(MK_MSG_POST_REPLY);
		selectable_p = news_p && gotBody && numindices == 1 && m_folder->AllowsPosting();
		break;
	case MSG_PostAndMailReply:
		display_string = XP_GetString(MK_MSG_POST_MAIL_REPLY);
		selectable_p = news_p && gotBody && numindices == 1 && m_folder->AllowsPosting();
		break;
	case MSG_ReplyToSender:
		display_string = XP_GetString(MK_MSG_REPLY_TO_SENDER);
		selectable_p = (numindices == 1) && gotBody;
		break;
	case MSG_ReplyToAll:
		display_string = XP_GetString(MK_MSG_REPLY_TO_ALL);
		selectable_p = (numindices == 1) && gotBody ;
		break;
	case MSG_ForwardMessageQuoted:
		display_string = XP_GetString(MK_MSG_FORWARD_QUOTED);
		selectable_p = (numindices == 1) && gotBody;
		break;
	case MSG_ForwardMessage:
	case MSG_ForwardMessageAttachment:
		selectable_p = (numindices > 0) && gotBody;
		display_string =  XP_GetString(MK_MSG_FORWARD);
		break;
	case MSG_SortByThread:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortByThread); 
		display_string = XP_GetString(MK_MSG_THREAD_MESSAGES);
		selected_used_p = TRUE;
		break;
	case MSG_SortByDate:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortByDate); 
		display_string = XP_GetString(MK_MSG_BY_DATE);
		selected_used_p = TRUE;
		break;
	case MSG_SortByMessageNumber:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortById);
		display_string = XP_GetString(MK_MSG_BY_MESSAGE_NB);
		selected_used_p = TRUE;
		break;
	case MSG_SortBySender:
		selectable_p = (m_fecount == 0);
		if (DisplayingRecipients())
			selected_p = (m_view->GetSortType() == SortByRecipient);
		else
			selected_p = (m_view->GetSortType() == SortByAuthor); 
		display_string = XP_GetString(MK_MSG_BY_SENDER);
		selected_used_p = TRUE;
		break;
	case MSG_SortBySubject:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortBySubject); 
		display_string = XP_GetString(MK_MSG_BY_SUBJECT);
		selected_used_p = TRUE;
		break;
	case MSG_SortByPriority:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortByPriority); 
		display_string = XP_GetString(MK_MSG_BY_PRIORITY);
		selected_used_p = TRUE;
		break;
	case MSG_SortByStatus:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortByStatus); 
		display_string = XP_GetString(MK_MSG_BY_STATUS);
		selected_used_p = TRUE;
		break;
	case MSG_SortBySize:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortBySize); 
		display_string = XP_GetString(MK_MSG_BY_SIZE);
		selected_used_p = TRUE;
		break;
	case MSG_SortByUnread:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortByUnread); 
		display_string = XP_GetString(MK_MSG_BY_UNREAD);
		selected_used_p = TRUE;
		break;
	case MSG_SortByFlagged:
		selectable_p = (m_fecount == 0);
		selected_p = (m_view->GetSortType() == SortByFlagged); 
		display_string = XP_GetString(MK_MSG_BY_FLAG);
		selected_used_p = TRUE;
		break;
	case MSG_SortBackward:
		selectable_p = (m_fecount == 0);
		selected_p = m_view->GetSortOrder() == SortTypeDescending; 
		display_string = XP_GetString(MK_MSG_SORT_BACKWARD);
		selected_used_p = TRUE;
		break;
	case MSG_ViewAllThreads :
		selected_p = m_view->GetViewType() == ViewAllThreads;
		display_string = XP_GetString(MK_MSG_ALIVE_THREADS);
		selected_used_p = TRUE;
		break;
	case  MSG_ViewKilledThreads:
		selected_p = m_view->GetShowingIgnored();
		selectable_p = (m_view->GetViewType() == ViewAllThreads);
		display_string = XP_GetString(MK_MSG_KILLED_THREADS);
		selected_used_p = TRUE;
		break;
	case MSG_ViewThreadsWithNew:		/* t: Show only threads with new messages */
		selected_p = m_view->GetViewType() == ViewOnlyThreadsWithNew;
		display_string = XP_GetString(MK_MSG_THREADS_WITH_NEW);
		selected_used_p = TRUE;
		break;
	case MSG_ViewWatchedThreadsWithNew: /* t: Show only watched thrds with new msgs */
		selected_p = m_view->GetViewType() == ViewWatchedThreadsWithNew;
		display_string = XP_GetString(MK_MSG_WATCHED_THREADS);
		selected_used_p = TRUE;
		break;
	case MSG_ViewNewOnly:
		selected_p = m_view->GetViewType() == ViewOnlyNewHeaders;
		display_string = XP_GetString(MK_MSG_NEW_MESSAGES_ONLY);
		selected_used_p = TRUE;
		break;
	case MSG_AddSender:
		display_string = XP_GetString(MK_MSG_ADD_SENDER_TO_ADDRESS_BOOK);
		selectable_p = TRUE;
		break;
	case MSG_AddAll:
		display_string = XP_GetString(MK_MSG_ADD_ALL_TO_ADDRESS_BOOK);
		selectable_p = TRUE;
		break;

	case MSG_DeleteMessage:
		{
		MSG_FolderInfo *trashFolder = NULL;
		GetMaster()->GetFoldersWithFlag (MSG_FOLDER_FLAG_TRASH, &trashFolder, 1);
		XP_ASSERT(trashFolder);
		selectable_p = numindices > 0;
		if (selectable_p)
			selectable_p = db->GetMailDB() != NULL; // can't delete news msg
		if (selectable_p) // Trash must exist and be unlocked
			selectable_p = trashFolder && !trashFolder->IsLocked(); 
		display_string = XP_GetString (MK_MSG_DELETE_SEL_MSGS);
		break;
		}
	case MSG_CollapseAll:
	case MSG_ExpandAll:
		selectable_p = (m_view->GetSize() > 0);
		break;
	case MSG_ToggleMessageRead:
		selectable_p = (numindices > 0);
		break;
	case MSG_ModerateNewsgroup:
		selectable_p = ModerateNewsgroupStatus (GetFolder());
		break;
	case MSG_NewCategory:
		selectable_p = NewNewsgroupStatus (GetFolder());
		break;
	default:
		return  MSG_Pane::GetCommandStatus( command, indices, numindices,
						   selectable_pP, selected_pP, display_stringP, plural_pP);
	}
	if (m_numstack > 0)			// if we're still performing a list changing command, disable
		selectable_p = FALSE;	// other commands, like sort while download new headers. 

	if (selectable_pP)
		*selectable_pP = selectable_p;
	if (selected_pP)
	{
		if (selected_used_p)
		{
			if (selected_p)
				*selected_pP = MSG_Checked;
			else
				*selected_pP = MSG_Unchecked;
		}
		else
		{
			*selected_pP = MSG_NotUsed;
		}
	}
	if (display_stringP)
		*display_stringP = display_string;
	if (plural_pP)
		*plural_pP = plural_p;

	return 0;
}


MsgERR MSG_ThreadPane::DataNavigate(MSG_MotionType motion,
						MessageKey startKey, MessageKey * resultKey, 
						MessageKey *resultThreadKey)
{
	XP_ASSERT(m_view);
	if (!m_view) return eNullView;

	return m_view->DataNavigate(startKey, motion,
		  resultKey, resultThreadKey);
}



void MSG_ThreadPane::NotifyPrefsChange(NotifyCode code) 
{
	if (((code == MailServerType) || (code == PopHost)) && (GetFolder()->GetType() == FOLDER_IMAPMAIL))
	  	FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderDeleted, (uint32)GetFolder());
}

// This is called when the current database or view has gone away,
// and the folder needs to be reloaded.
MsgERR MSG_ThreadPane::ReloadFolder()
{
	MSG_FolderInfo *f = m_folder;
	MsgERR err = eSUCCESS;

//	XP_ASSERT(m_view == NULL);
	CloseView();	// why not just make sure it's closed?

	m_folder = NULL;
	err = LoadFolder(f);
	return err;
}


MsgERR MSG_ThreadPane::FinishLoadingIMAPUpgradeFolder(XP_Bool reallyLoad)
{
	if (m_imapUpgradeBeginFolder)
	{
		if (reallyLoad)
			LoadFolder(m_imapUpgradeBeginFolder);
		m_imapUpgradeBeginFolder = 0;
	}
	return eSUCCESS;
}

MsgERR
MSG_ThreadPane::LoadFolder(const char *url)
{
	MsgERR	err = eSUCCESS;
	int urlType = NET_URL_Type(url);
	MSG_FolderInfo	*folder = NULL;
	char *folderName = NULL;
	char *host_and_port = NULL;

	switch (urlType)
	{
	case NEWS_TYPE_URL:
	{
		folderName = NewsGroupDB::GetGroupNameFromURL(url);
		host_and_port = NET_ParseURL(url, GET_HOST_PART);

		folder = m_master->FindNewsFolder(host_and_port, folderName,
										  *url == 's');
	}
		break;
	case IMAP_TYPE_URL:
	{
		folderName = NET_ParseURL(url, GET_PATH_PART);
		folder = m_master->FindMailFolder(folderName, FALSE);
		break;
	}
	case MAILBOX_TYPE_URL:
	{
		folderName = NET_ParseURL(url, GET_PATH_PART);
		folder = m_master->FindMailFolder(folderName, FALSE);
		break;
	}
	default:
		break;
	}

	if (folder != NULL)
		err = LoadFolder(folder);

	FREEIF(folderName);
	FREEIF(host_and_port);
	return err;
}

MSG_FolderInfo* MSG_ThreadPane::GetFolder() 
{
	return m_folder;
}

void MSG_ThreadPane::SetFolder(MSG_FolderInfo *info)
{
	m_folder = info;
}


PaneListener *MSG_ThreadPane::GetListener() 
{
	return &m_threadPaneListener;
}

MsgERR
MSG_ThreadPane::LoadFolder(MSG_FolderInfo* f) 
{
	char	*url = NULL;
	XP_Bool	calledEndingUpdate = FALSE;
	XP_Bool	finishedLoadingFolder = FALSE;	// are we finished loading folder?
	
	// Is the folder we're loading going to be a mail fcc, news fcc folder,
	// or a child of one of those? We don't know yet, so make sure we go
	// figure it out if the FE asks
	m_displayRecipients = msg_DontKnow;

	if (f == m_folder) 
	{
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) f);
		return 0;
	}

	// switching folders - clear out old folder
	if (m_folder)
	{
		ClearNewBits(FALSE);
	}
	m_folder = f;

	if (!f)
		return CloseView();

	XP_Bool serverFolder = (f->GetFlags() & MSG_FOLDER_FLAG_NEWS_HOST || f->GetType() == FOLDER_IMAPSERVERCONTAINER 
		|| (f->GetFlags() & MSG_FOLDER_FLAG_DIRECTORY && f->GetDepth() < 2));

	// we need to abort any current background parsing.
	if (m_folder != NULL)
		msg_InterruptContext(GetContext(), TRUE); 

	if (m_folder && m_folder->UserNeedsToAuthenticateForFolder(FALSE) && m_master->PromptForHostPassword(GetContext(), m_folder) != 0)
		return 0;

	FEStart();
	StartingUpdate(MSG_NotifyAll, 0, 0);

	CloseView();

	MSG_IMAPHost *host = NULL;
	MSG_IMAPFolderInfoMail *imapInfo = f->GetIMAPFolderInfoMail();
	if (imapInfo)
	{
		host = imapInfo->GetIMAPHost();
	}

	if (m_master->GetUpgradingToIMAPSubscription() || m_master->TryUpgradeToIMAPSubscription(GetContext(), FALSE, this, host))
	{
		EndingUpdate(MSG_NotifyAll, 0, 0);
		FEEnd();
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, MSG_VIEWINDEXNONE);
		if (GetMaster()->ShowIMAPProgressWindow())
		{
			m_imapUpgradeBeginFolder = m_folder;
		}
		m_folder = 0;
		return eSUCCESS;
	}

	if (serverFolder)
	{
		EndingUpdate(MSG_NotifyAll, 0, 0);
		FEEnd();
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) f);
		return eSUCCESS;
	}

	url = f->BuildUrl(NULL, MSG_MESSAGEKEYNONE);

	ViewType typeOfView = ViewAny;
#ifdef DEBUG_bienvenu
	if (f->GetType() == FOLDER_IMAPMAIL && !XP_STRCMP(f->GetName(), "r-thompson"))
		typeOfView = ViewCacheless;
#endif
	
	if (f->IsNews() || (f->GetType() == FOLDER_IMAPMAIL))
	{
		// Tell fe we're done before we open the view so fe won't
		// select the first message until we've finished downloading new
		// headers. Otherwise, our url will get interrupted
		EndingUpdate(MSG_NotifyAll, 0, 0);
		FEEnd();
		calledEndingUpdate = TRUE;
	}

	if (imapInfo)
	{
		if (!imapInfo->GetCanIOpenThisFolder())	// NoSelect, or from ACLs
		{
			FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) f);
			return eSUCCESS;
		}
	}

	MsgERR status = MessageDBView::OpenURL(url, GetMaster(),
						 				   typeOfView, &m_view, FALSE);

	
	// fire off the IMAP url to update this folder, if it is IMAP
	if ((f->GetType() == FOLDER_IMAPMAIL) && (status == eSUCCESS))
	{
		MSG_IMAPFolderInfoMail *imapFolder = f->GetIMAPFolderInfoMail();
		if (imapFolder)
		{
			imapFolder->StartUpdateOfNewlySelectedFolder(this,TRUE,NULL);
		}
		else
		{
			XP_ASSERT(FALSE);
		}
	}
	
	if (f->GetType() == FOLDER_MAIL && status != eSUCCESS)
	{
		if (status == eBuildViewInBackground)
		{
			status = ListThreads();
		}

		if (status != eSUCCESS /*== eOldSummaryFile*/)
		{
			MailDB	*mailDB;

			const char *pathname = ((MSG_FolderInfoMail*) f)->GetPathname();
			status = MailDB::CloneInvalidDBInfoIntoNewDB(pathname, &mailDB);
			if (status == eSUCCESS || status == eNoSummaryFile)
			{
				MSG_Master *mailMaster = GetMaster();
				URL_Struct *url_struct;
				url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);

				if (mailDB != NULL && (status == eSUCCESS || status == eNoSummaryFile))
				{
					status = MessageDBView::OpenViewOnDB(mailDB, 
						ViewAny, &m_view);
					mailDB->Close(); // decrement ref count.
				}
				if (m_view != NULL && m_view->GetDB() != NULL)
				{
					ParseMailboxState *parseMailboxState = new ParseMailboxState(pathname);
					parseMailboxState->SetView(m_view);  
					parseMailboxState->SetPane(this);
					parseMailboxState->SetIncrementalUpdate(FALSE);
					parseMailboxState->SetMaster(mailMaster);
					parseMailboxState->SetDB(m_view->GetDB()->GetMailDB());
					parseMailboxState->SetContext(GetContext());
					parseMailboxState->SetFolder(m_folder);
					((MSG_FolderInfoMail*)f)->SetParseMailboxState(parseMailboxState);
					// fire off url to create summary file from mailbox - will work in background
					//GetURL(url_struct, FALSE);
					MSG_UrlQueue::AddUrlToPane (url_struct, NULL, this);
				}
			}
		}
	}
	else if (f->IsNews())
	{
		MSG_FolderInfoNews *newsFolder = f->GetNewsFolderInfo();
		XP_ASSERT(newsFolder);
		const char* groupname = newsFolder->GetNewsgroupName();
		if (status != eSUCCESS)
		{
			if (status == eOldSummaryFile || status == eErrorOpeningDB)
			{
				NewsGroupDB	*newsDB;
				const char *dbFileName = newsFolder->GetXPDBFileName();

				if (dbFileName != NULL)
					XP_FileRemove(dbFileName, xpXoverCache);
				status = NewsGroupDB::Open(url, GetMaster(), &newsDB);
				if (status == eSUCCESS)
				{
					status = MessageDBView::OpenViewOnDB(newsDB, 
						ViewAny, &m_view);
					newsDB->Close();
				}
			}
		}
		if (status == eSUCCESS)	// get new articles from server.
		{
			MSG_Master *master = GetMaster();
			URL_Struct *url_struct;
			url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
			ListNewsGroupState * listState = new ListNewsGroupState(url, groupname, this);
			listState->SetMaster(master);
			listState->SetView(m_view);
			newsFolder->SetListNewsGroupState(listState);
			int status = GetURL(url_struct, FALSE);
			if (status == MK_INTERRUPTED || status == MK_OFFLINE)	
			{
				if (newsFolder->GetListNewsGroupState())	// FinishXOver not called...
				{
					delete listState;
					newsFolder->SetListNewsGroupState(NULL);
				}
				StartingUpdate(MSG_NotifyAll, 0, 0);
				EndingUpdate(MSG_NotifyAll, 0, 0);
				finishedLoadingFolder = TRUE;
			}
		}
		else if (status == eBuildViewInBackground)
		{
			status = ListThreads();
		}
	}
	else if (f->GetType() == FOLDER_IMAPMAIL && status != eSUCCESS)
	{
		const char* pathname = ((MSG_FolderInfoMail*) f)->GetPathname();
		XP_FileRemove(pathname, xpMailFolderSummary);
	}
	else if (status == eSUCCESS)
	{
		finishedLoadingFolder = TRUE;
	}
	if (status == eSUCCESS)
	{
		m_view->Add(&m_threadPaneListener);
	}

	if (!calledEndingUpdate)
	{
		EndingUpdate(MSG_NotifyAll, 0, 0);
		FEEnd();
	}

	// imap folders loads are asynchronous
	// this notification will happen in
	// MSG_IMAPFolderInfoMail::AllFolderHeadersAreDownloaded
	if (finishedLoadingFolder && (f->GetType() != FOLDER_IMAPMAIL))
	{
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) f);
		// the folder loaded sync notification is for the xfe so that it knows a folder
		// has loaded w/o being in a running url.
#ifdef XP_UNIX
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoadedSync, (uint32) f);
#endif
	}


	if (m_folder)
		m_folder->SummaryChanged();	// So thread pane can display counts.

	XP_FREE(url);
	return status;
}

char *
MSG_ThreadPane::BuildUrlForKey (MessageKey key)
{
	return m_folder->BuildUrl (m_view->GetDB(), key);
}


XP_Bool
MSG_ThreadPane::GetThreadLineByIndex(MSG_ViewIndex line, int32 numlines,
									 MSG_MessageLine* data) 
{
	if (!m_view || !m_view->GetDB()) return FALSE;
	int numlisted = -1;
	MsgERR status;

	if (m_folder->UserNeedsToAuthenticateForFolder(TRUE))
		return FALSE;

	if (!DisplayingRecipients())
	{
		status = m_view->ListShortMsgHdrByIndex(line, numlines, data,
											   &numlisted);
		if (status != 0 || numlisted != numlines) return FALSE;
	}

	else if (DisplayingRecipients())
	{
		MessageHdrStruct	msgHdr;

		for (int i = 0; i < numlines; i++)
		{
			status = m_view->ListMsgHdrByIndex(line + i, 1, &msgHdr,
											   &numlisted);

			if (status != 0 || numlisted != 1) 
				break;
			MessageDB::CopyFullHdrToShortHdr(&data[i], &msgHdr);
			XP_STRNCPY_SAFE(data[i].author, msgHdr.m_recipients, sizeof(data[i].author));
		}
	}
	return TRUE;
}

int
MSG_ThreadPane::GetThreadLevelByIndex(MSG_ViewIndex line)
{
	if (!m_view) return FALSE;
	int level = 0;
	MsgERR status;

	status =  m_view->GetMsgLevelByIndex(line, level);
	if (!status)
		return level;
	else
		return 0;
}

XP_Bool
MSG_ThreadPane::GetThreadLineByKey(MessageKey key, MSG_MessageLine* data) 
{
	XP_ASSERT(m_view);
	if (!m_view) return FALSE;
	int numlisted = -1;
	MsgERR status = m_view->ListThreadsShort(&key, 1, data, &numlisted);
	return (status == 0 && numlisted == 1);
}

void
MSG_ThreadPane::ToggleRead(int line) 
{
	XP_ASSERT(m_view);
	m_view->ToggleReadByIndex(line);
}


MSG_ViewIndex 
MSG_ThreadPane::GetMessageIndex(MessageKey key, XP_Bool expand /* = FALSE */)
{
	MSG_ViewIndex retIndex = MSG_VIEWINDEXNONE;
	if (m_view)
		retIndex =  m_view->FindKey(key, expand);

	return retIndex;;
}


MessageKey
MSG_ThreadPane::GetMessageKey(MSG_ViewIndex index)
{
	if (!m_view) return MSG_VIEWINDEXNONE;
	return m_view->GetAt(index);
}

// These routines need to deal with getting non-thread indexes.
void
MSG_ThreadPane::ToggleExpansion(MSG_ViewIndex line, int32* numchanged)
{
	int32 ourNumChanged;

	MsgERR err = m_view->ToggleExpansion(line, (uint32 *) &ourNumChanged);
	if (err == eSUCCESS)
	{
		if (numchanged != 0)
			*numchanged = ourNumChanged;
	}
}
  

int32
MSG_ThreadPane::ExpansionDelta(MSG_ViewIndex line)
{
	int32 expansionDelta;

	XP_ASSERT(m_view);
	if (!m_view) return 0;
	MsgERR err = m_view->ExpansionDelta(line, &expansionDelta);
	return (err == eSUCCESS) ? expansionDelta : 0;
}


int32
MSG_ThreadPane::GetNumLines()
{
	if (!m_view) return 0;	// Pretend no view is empty pane
	return m_view->GetSize();
}

/*static*/
void MSG_ThreadPane::ViewChange(MessageDBView * /*dbView*/,
								MSG_ViewIndex line_number,
								MessageKey /*key*/, int num_changed,
								MSG_NOTIFY_CODE code,
								void *closure)
{
	MSG_ThreadPane *pane = (MSG_ThreadPane *) closure;

	pane->StartingUpdate(code, line_number, num_changed);
	pane->EndingUpdate(code, line_number, num_changed);
}

MsgERR MSG_ThreadPane::OpenDraft (MSG_FolderInfo *folder, MessageKey key)
{
  /****
	XP_ASSERT ( folder->GetFlags() & MSG_FOLDER_FLAG_DRAFTS );
  ****/
	char *url = BuildUrlForKey (key);
	URL_Struct* url_struct;
	MSG_PostDeliveryActionInfo *actionInfo = 0;

	if (NULL != url)
	{
		url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
		XP_FREE (url);
		if (!url_struct)
		  /***** turn me to MsgERR please */
		  return MK_OUT_OF_MEMORY; 

		if (folder->GetFlags() & MSG_FOLDER_FLAG_DRAFTS ||
			folder->GetFlags() & MSG_FOLDER_FLAG_QUEUE)
		{
			actionInfo = new MSG_PostDeliveryActionInfo (folder) ;
			/*
			 * actionInfo will be freed by CompositionPane
			 */
			if (!actionInfo) {
			  NET_FreeURLStruct (url_struct);
			  /**** turn me to MsgERR please */
			  return MK_OUT_OF_MEMORY;
			}
			
			actionInfo->m_msgKeyArray.Add(key);
			actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
		}

		url_struct->fe_data = (void *) actionInfo;
		url_struct->msg_pane = this;
		url_struct->allow_content_change = FALSE;
#if 0
		NET_GetURL (url_struct, FO_OPEN_DRAFT, m_context,
			MSG_ThreadPane::OpenDraftExit);
#else
		MSG_UrlQueue::AddUrlToPane (url_struct, MSG_ThreadPane::OpenDraftExit, this, TRUE, FO_OPEN_DRAFT);
#endif

		return eSUCCESS;
	}
	else
		return eUNKNOWN; //PHP need better error code
}

void MSG_ThreadPane::OpenDraftExit (URL_Struct *url_struct,
									int /*status*/,
									MWContext* context)
{
	XP_ASSERT (url_struct && context);

	if (!url_struct) return;

	NET_FreeURLStruct ( url_struct );
}


MsgERR
MSG_ThreadPane::ComposeMessage(MSG_CommandType command, MSG_ViewIndex* indices,
							   int32 numIndices)
{
	XP_ASSERT(numIndices == 1);
	if (numIndices != 1) return eUNKNOWN;
	MSG_MessagePane* mpane;
	MessageKey key = GetMessageKey(indices[0]);
	XP_ASSERT(key != MSG_MESSAGEKEYNONE);
	if (key == MSG_MESSAGEKEYNONE) return eUNKNOWN;
	for (mpane = (MSG_MessagePane*) m_master->FindFirstPaneOfType(MSG_MESSAGEPANE);
		 mpane;
		 mpane = (MSG_MessagePane*) m_master->FindNextPaneOfType(mpane->GetNextPane(),
																 MSG_MESSAGEPANE)) 
	{
		XP_ASSERT(mpane->GetPaneType() == MSG_MESSAGEPANE);
		if (mpane->GetPaneType() != MSG_MESSAGEPANE) continue;
		MSG_FolderInfo* f;
		MessageKey k;
		mpane->GetCurMessage(&f, &k, NULL);
		if (f == m_folder && k == key) {
			return mpane->ComposeMessage(command);
		}
	}
	mpane = new MSG_MessagePane(m_context, m_master);
	if (!mpane) return eOUT_OF_MEMORY;
	HG83734
	return mpane->MakeComposeFor(m_folder, key, command);
}



MsgERR
MSG_ThreadPane::ForwardMessages(MSG_ViewIndex* indices, int32 numIndices)
{
	MsgERR status;
	if (numIndices == 1) {
		return ComposeMessage(MSG_ForwardMessageAttachment, indices, numIndices);
	}
	if (numIndices < 1) return 0;

	MessageHdrStruct header;
	status = m_view->GetDB()->GetMessageHdr(GetMessageKey(indices[0]), &header);
	if (status != eSUCCESS) return status;
	char* fwd_subject = CreateForwardSubject(&header);

	MSG_CompositionFields* fields = new MSG_CompositionFields();
	fields->SetSubject(fwd_subject);
	FREEIF(fwd_subject);

	MSG_PostDeliveryActionInfo *actionInfo = 0;
	actionInfo = new MSG_PostDeliveryActionInfo(GetFolder());
	if (actionInfo) {
		actionInfo->m_flags |= MSG_FLAG_FORWARDED;
	}

	for (int32 i=0 ; i<numIndices ; i++) {
		char* tmp = BuildUrlForKey(GetMessageKey(indices[i]));
		if (tmp) {
			fields->AddForwardURL(tmp);
			XP_FREE(tmp);
		}
		if (actionInfo) {
		  actionInfo->m_msgKeyArray.Add(GetMessageKey(indices[i]));
		}
	}

	XP_Bool prefBool = FALSE;
	PREF_GetBoolPref("mail.attach_vcard",&prefBool);
	fields->SetAttachVCard(prefBool);

	/* Note that forwarding multiple messages takes a totally
	   different code path than forwarding a single message.
	 */
	HG29832
	if (!fields->GetSigned())
	  fields->SetSigned(MSG_GetMailSigningPreference());

	MSG_CompositionPane* comppane =	(MSG_CompositionPane*) 
	  FE_CreateCompositionPane(m_context, fields, NULL, MSG_DEFAULT);
	if (!comppane) {
		delete fields;
		if (actionInfo)
		  delete actionInfo;
	}
	else if (actionInfo) {
	  MSG_SetPostDeliveryActionInfo((MSG_Pane *)comppane,
									(void *) actionInfo);
	}
	return 0;
}
										
