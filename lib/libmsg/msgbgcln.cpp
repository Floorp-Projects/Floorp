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

#include "msgbgcln.h"
#include "msgcmfld.h"
#include "msgpane.h"
#include "msgfinfo.h"
#include "msgpurge.h"
#include "msgimap.h"

MSG_BackgroundCleanup::MSG_BackgroundCleanup(XPPtrArray &foldersToCleanup)
{
	m_foldersToCleanup.InsertAt(0, &foldersToCleanup);
	m_currentFolderCompressor = 0;
	m_currentNewsgroupPurger = 0;
}

MSG_BackgroundCleanup::~MSG_BackgroundCleanup()
{
	delete m_currentFolderCompressor;
	delete m_currentNewsgroupPurger;
}

int MSG_BackgroundCleanup::DoSomeWork()
{
	int status;

	switch (m_state)
	{
	case BGCleanupStart:
		m_state = BGCleanupNextGroup;
		break;
	case BGCleanupNextGroup:
		if (m_folderIndex >= m_foldersToCleanup.GetSize())
			return MK_CONNECTED;
		else
			m_folder = m_foldersToCleanup.GetAt(m_folderIndex++);
		m_bytesTotal = m_bytesCompressed = 0;
		if (m_folder->IsMail())
		{
			MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
			// only try to cleanup this folder if the user is authenticated
			if (imapFolder && m_pane->GetMaster()->IsUserAuthenticated())
			{
				// this will add the compress url to the url queue we are running in.
				imapFolder->ClearRequiresCleanup();	// pretend it's going to work.
				if (imapFolder->GetFlags() & MSG_FOLDER_FLAG_TRASH)
					imapFolder->DeleteAllMessages(m_pane, FALSE);
				else if (m_pane->GetMaster()->HasCachedConnection(imapFolder))
					m_pane->CompressOneMailFolder(imapFolder);
			}
			else
			{
				m_currentFolderCompressor = 
					new MSG_CompressFolderState(m_pane->GetMaster(), m_pane->GetContext(), NULL, 
												(MSG_FolderInfoMail *)m_folder,
												m_bytesCompressed, 
												m_bytesTotal);

				status = m_currentFolderCompressor->BeginCompression();
			}
		}
		else if (m_folder->IsNews())
		{
			// ### dmb check errors
			m_currentNewsgroupPurger = new MSG_PurgeNewsgroupState(m_pane, m_folder);
			m_currentNewsgroupPurger->Init();
		}
		m_state = BGCleanupMoreHeaders;
		break;
	case BGCleanupMoreHeaders:
	{
		if (m_folder->IsMail())
		{
			MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
			if (imapFolder)
			{
				m_state = BGCleanupNextGroup;
				status = 0;
			}
			else
			{
				status = m_currentFolderCompressor->CompressSomeMore();
				if (status != MK_WAITING_FOR_CONNECTION)
				{
					if (status < 0) 
					{
						char* pString = XP_GetString(status);
						if (pString && strlen(pString))
							FE_Alert(m_pane->GetContext(), pString);
					}
					m_state = BGCleanupNextGroup;
					status = StopCompression();
				}
			}
		}
		else if (m_folder->IsNews())
		{
			status = m_currentNewsgroupPurger->PurgeSomeMore();
			if (status != MK_WAITING_FOR_CONNECTION)
			{
				m_state = BGCleanupNextGroup;
				status = StopPurging();
			}
		}
	}
		break;
	default:
		break;
	}
	return MK_WAITING_FOR_CONNECTION;
}

int MSG_BackgroundCleanup::StopCompression()
{
	int status = m_currentFolderCompressor->FinishCompression();
	delete m_currentFolderCompressor;
	m_currentFolderCompressor = NULL;
	return status;
}

int MSG_BackgroundCleanup::StopPurging()
{
	int status = m_currentNewsgroupPurger->FinishPurging();
	delete m_currentNewsgroupPurger;
	m_currentNewsgroupPurger = NULL;
	return status;
}


XP_Bool MSG_BackgroundCleanup::AllDone(int /*status*/)
{
	if (m_folder)
	{
		if (m_folder->IsMail() && m_currentFolderCompressor)
			StopCompression();
		else if (m_folder->IsNews() && m_currentNewsgroupPurger)
			StopPurging();

	}
	return TRUE;
}

int MSG_BackgroundCleanup::DoNextFolder()
{
	return 0;
}

int MSG_BackgroundCleanup::DoProcess()
{
	return 0;
}

