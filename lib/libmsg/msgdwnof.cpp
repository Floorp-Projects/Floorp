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
#include "msg.h"
#include "msgdwnof.h"
#include "pmsgsrch.h"
#include "msgfinfo.h"
#include "msgurlq.h"
#include "nwsartst.h"
#include "newsdb.h"
#include "listngst.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
}

MSG_DownloadOfflineFoldersState::MSG_DownloadOfflineFoldersState(MSG_Pane *pane)
{
	m_folderIterator = new MSG_FolderIterator(pane->GetMaster()->GetFolderTree());
	m_pane = pane;
	m_newsDB = NULL;
	m_oneFolder = FALSE;
	m_foundFolderToDownload = FALSE;
	m_curFolder = NULL;
}

MSG_DownloadOfflineFoldersState::~MSG_DownloadOfflineFoldersState()
{
	delete m_folderIterator;
}

int		MSG_DownloadOfflineFoldersState::DoIt()
{
	return DoSomeMore();
}


/* static */int MSG_DownloadOfflineFoldersState::FolderDoneCB(void *state, int /* status */)
{
	MSG_DownloadOfflineFoldersState	*downloadState = (MSG_DownloadOfflineFoldersState *) state;

	return downloadState->DoSomeMore();
}

int MSG_DownloadOfflineFoldersState::DownloadOneFolder(MSG_FolderInfo *folder)
{
	m_oneFolder = TRUE;
	m_folderIterator->AdvanceToFolder(folder);
	return DownloadFolder(folder);
}
int MSG_DownloadOfflineFoldersState::DownloadFolder(MSG_FolderInfo *folder)
{
	char *url = folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	if (!url)
		return eOUT_OF_MEMORY;

	m_curFolder = folder;
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	XP_ASSERT(newsFolder);
	const char* groupname = newsFolder->GetNewsgroupName();
	MSG_Master *master = m_pane->GetMaster();
	URL_Struct *url_struct;
	url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	ListNewsGroupState * listState = new ListNewsGroupState(url, groupname, m_pane);

	XPPtrArray referringPanes;

	master->FindPanesReferringToFolder(newsFolder, &referringPanes);

	for (int i = 0; i < referringPanes.GetSize(); i++)
	{
		MSG_Pane *pane = (MSG_Pane *) referringPanes.GetAt(i);
		if (pane && pane->GetMsgView())
		{
			listState->SetView(pane->GetMsgView());
			break;
		}
	}

	listState->SetMaster(master);
	listState->SetGetOldMessages(FALSE);	// get messages below highwater mark if we don't have them
	newsFolder->SetListNewsGroupState(listState);
	url_struct->fe_data = this;
	url_struct->msg_pane = m_pane;
	MSG_UrlQueue::AddUrlToPane (url_struct, MSG_DownloadOfflineFoldersState::DownloadArticlesCB, m_pane, TRUE, FO_PRESENT);
	return eSUCCESS;
}

/* static */ void MSG_DownloadOfflineFoldersState::DownloadArticlesCB(URL_Struct *url , int status, MWContext * /* context */)
{
	if (status != MK_INTERRUPTED)
	{
		MSG_DownloadOfflineFoldersState *downloadState =
			(MSG_DownloadOfflineFoldersState *) url->fe_data;
		XP_ASSERT (downloadState);
		if (downloadState)
			downloadState->DownloadArticles(downloadState->GetCurFolder());
	}
#ifdef DEBUG_bienvenu
	else
		XP_Trace("download articles interrupted\n");
#endif
}

int MSG_DownloadOfflineFoldersState::DownloadArticles(MSG_FolderInfo *curFolder)
{
	XP_Bool useDefaults, byReadness, unreadOnly, byDate;
	int32	daysOld;
	int	ret = 0;

	MSG_GetOfflineRetrievalInfo(curFolder, &useDefaults, &byReadness, &unreadOnly, &byDate, &daysOld);
	if (useDefaults)
	{
		// ### dmb find preferences! Change local variables above based on them.
	}
	MSG_SearchTermArray termArray;
	MSG_SearchValue		value;
	if (byReadness && unreadOnly)
	{
		value.attribute = attribMsgStatus;
		value.u.msgStatus = kIsRead;
		termArray.Add(new MSG_SearchTerm(attribMsgStatus, opIsnt, &value, MSG_SearchBooleanAND, NULL));
	}
	if (byDate)
	{
		value.attribute = attribAgeInDays;
		value.u.age = daysOld;
		termArray.Add(new MSG_SearchTerm(attribAgeInDays, opIsLessThan, &value, MSG_SearchBooleanAND, NULL));
	}
	if (termArray.GetSize() == 0)	// cons up a search term that will match every msg we need
	{
		value.attribute = attribMsgStatus;
		value.u.msgStatus = kOffline;
		termArray.Add(new MSG_SearchTerm(attribMsgStatus, opIsnt, &value, MSG_SearchBooleanAND, NULL));
	}
	char *url = curFolder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	if (!url)
		return MK_OUT_OF_MEMORY;
	
	MsgERR err = NewsGroupDB::Open(url, m_pane->GetMaster(), &m_newsDB);
	if (err == eSUCCESS)
		ret = DownloadMatchingNewsArticlesToNewsDB::SaveMatchingMessages(m_pane->GetContext(), curFolder, 
				m_newsDB, termArray, FolderDoneCB, this);

	for (int32 index = 0; index < termArray.GetSize(); index++)
	{
		MSG_SearchTerm *doomedSearchTerm = termArray.GetAt(index);
		if (doomedSearchTerm)
			delete doomedSearchTerm;
	}
	return ret;

}
int MSG_DownloadOfflineFoldersState::DoSomeMore()
{
	int	ret = 0;
	MSG_FolderInfo *nextFolder = m_folderIterator->Next();
	XP_Bool		prevFolderWasCategory = (m_curFolder) ?  (m_curFolder->GetFlags() & MSG_FOLDER_FLAG_CATEGORY) != 0 : FALSE;

	if (m_newsDB != NULL)
		m_newsDB->Close();
	m_newsDB = NULL;
	if (m_oneFolder  && !prevFolderWasCategory)	// if we're only doing one folder, we're done.
		return ret;
	while (nextFolder)
	{
		MSG_FolderInfo *nextCategoryContainer = (prevFolderWasCategory) ? MSG_GetCategoryContainerForCategory(nextFolder) : 0;
		uint32 folderPrefFlags = (nextCategoryContainer) 
			? nextCategoryContainer->GetFolderPrefFlags() : nextFolder->GetFolderPrefFlags();

		// if we're doing one folder, and the category container has changed, we're done
		if (m_oneFolder && prevFolderWasCategory && 
			nextCategoryContainer != MSG_GetCategoryContainerForCategory(m_curFolder))
				return ret;

		MSG_FolderInfoNews *newsFolder = nextFolder->GetNewsFolderInfo();
		if (newsFolder && newsFolder->IsSubscribed() &&	((folderPrefFlags & MSG_FOLDER_PREF_OFFLINE) != 0) || m_oneFolder)
		{
			ret = DownloadFolder(nextFolder);
			m_foundFolderToDownload = TRUE;
			break;
		}
		else
			nextFolder = m_folderIterator->Next();
	}
	ret = (m_foundFolderToDownload) ? ret : -1;	// didn't find one folder!
	if (nextFolder == NULL || ret == MK_CONNECTED)
	{
		if (m_pane && m_pane->GetURLChain())
			m_pane->GetURLChain()->GetNextURL();
		delete this;	// would be nice to do this at a higher level.
	}
	return ret;
}
