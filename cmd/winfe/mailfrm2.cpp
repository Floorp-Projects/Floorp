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

//
// Main frame for mail reading window
//
#include "stdafx.h"
#include "mailfrm.h"
#include "filter.h"
#include "srchfrm.h"
#include "mnwizard.h"
#include "prefs.h"
#include "dspppage.h"
#include "fldrfrm.h"
#include "thrdfrm.h"

void CMailNewsFrame::OnSearch()
{
	CSearchFrame::Open( this );
}

void CMailNewsFrame::OnFilter()
{
	CFilterPickerDialog dlgFilter(m_pPane, this);
	dlgFilter.DoModal();
}

BOOL CMailNewsFrame::CheckWizard( CWnd *pParent )
{
	if (pParent == NULL)
		pParent = GetActiveWindow();
	if (!g_MsgPrefs.IsValid())
	{
		CMailNewsWizard setupWizard( pParent );

		if(setupWizard.DoModal() == IDCANCEL)
			return FALSE;  
	}
	return TRUE;
}

void CMailNewsFrame::OnEditProperties()
{
	MSG_ViewIndex *indices = NULL;
	int count = 0;
	BOOL bNoPropertiesNeeded = FALSE;
	MSG_FolderLine folderLine;

	CMailNewsOutliner* pOutliner;
	MSG_Pane* pFolderPane;
	if (IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		pOutliner = ((C3PaneMailFrame*)this)->GetFolderOutliner();
		pFolderPane = ((C3PaneMailFrame*)this)->GetFolderPane();
	}
	else
	{
		pOutliner = m_pOutliner;
		pFolderPane = m_pPane;
	}
	
	if (pOutliner)
		pOutliner->GetSelection( indices, count );

	if (indices && count)
	{
		MSG_FolderInfo *folderInfo = MSG_GetFolderInfo( pFolderPane, indices[0] );
		MSG_GetFolderLineByIndex( pFolderPane, indices[0], 1, &folderLine );

		CString csTitle;
		if (folderLine.flags & (MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_CATEGORY)) {
			csTitle.LoadString(IDS_NEWSGROUPPROP);
		} else if (folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST) {
			csTitle.LoadString(IDS_NEWSHOSTPROP);
		} else {
			if (folderLine.level > 1) {
				csTitle.LoadString(IDS_FOLDERPROP);
			}else {
				csTitle.LoadString(IDS_MAILSERVERPROP);
			}
		}
		CNewsFolderPropertySheet FolderSheet( csTitle, this );
		//destructor handles clean up of added sheets

		//only make and add the pages if they selected a news group or category
		if ( folderLine.flags & (MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_CATEGORY) )
		{
			FolderSheet.m_pNewsFolderPage= new CNewsGeneralPropertyPage(&FolderSheet);
			FolderSheet.m_pNewsFolderPage->SetFolderInfo( folderInfo, (MWContext*)GetContext() );

			FolderSheet.m_pDownLoadPageNews = new CDownLoadPPNews(&FolderSheet);
			FolderSheet.m_pDownLoadPageNews->SetFolderInfo(folderInfo);

			FolderSheet.m_pDiskSpacePage = new CDiskSpacePropertyPage(&FolderSheet);
			FolderSheet.m_pDiskSpacePage->SetFolderInfo (folderInfo );

			FolderSheet.AddPage(FolderSheet.m_pNewsFolderPage);
			FolderSheet.AddPage(FolderSheet.m_pDownLoadPageNews);
			FolderSheet.AddPage(FolderSheet.m_pDiskSpacePage);
		}
		else if ( folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST) 
		{
			MSG_NewsHost *pNewsHost = MSG_GetNewsHostFromIndex (pFolderPane, indices[0]);
			FolderSheet.m_pNewsHostPage= new CNewsHostGeneralPropertyPage;
			FolderSheet.m_pNewsHostPage->SetFolderInfo( folderInfo,pNewsHost );
			FolderSheet.AddPage(FolderSheet.m_pNewsHostPage);
		}
		else if ( folderLine.level > 1)
		{
			FolderSheet.m_pFolderPage= new CFolderPropertyPage(&FolderSheet);
			FolderSheet.m_pFolderPage->SetFolderInfo( folderInfo, pFolderPane );
			FolderSheet.AddPage(FolderSheet.m_pFolderPage);
			//only add this page if it's an offline mail folder.
			if (folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX)
			{
				FolderSheet.m_pSharingPage= new CFolderSharingPage(&FolderSheet);
				FolderSheet.m_pSharingPage->SetFolderInfo(folderInfo, 
												pFolderPane, (MWContext*)GetContext());
				FolderSheet.AddPage(FolderSheet.m_pSharingPage);

				FolderSheet.m_pDownLoadPageMail = new CDownLoadPPMail;
				FolderSheet.m_pDownLoadPageMail->SetFolderInfo( folderInfo );
				FolderSheet.AddPage(FolderSheet.m_pDownLoadPageMail);
			}
		}
		else
		{
			wfe_DisplayPreferences((CGenericFrame*)this);
			bNoPropertiesNeeded = TRUE;
		}

		if ( !bNoPropertiesNeeded)
		{
			if(FolderSheet.DoModal() == IDOK)
			{
				if(FolderSheet.DownLoadNow())
				{
					new CProgressDialog(this, NULL,_ShutDownFrameCallBack,
							folderInfo,szLoadString(IDS_DOWNLOADINGARTICLES));
					;//DonwLoad!!!!!!!
				}
				else if (FolderSheet.CleanUpNow())
				{
					 MSG_Command (pFolderPane, MSG_CompressFolder,
								indices, 1);
				}
				else if (FolderSheet.SynchronizeNow())
						;//Synchronize!!!!
			}
		}
	}
}

void CMailNewsFrame::OnUpdateProperties( CCmdUI *pCmdUI )
{
	MSG_ViewIndex *indices = NULL;
	int count = 0;
	MSG_FolderLine folderLine;

	if (pCmdUI)
	{   
		CMailNewsOutliner* pOutliner;
		MSG_Pane* pFolderPane;
		if (IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
		{
			pOutliner = ((C3PaneMailFrame*)this)->GetFolderOutliner();
			pFolderPane = ((C3PaneMailFrame*)this)->GetFolderPane();
		}
		else
		{
			pOutliner = m_pOutliner;
			pFolderPane = m_pPane;
		}
		pOutliner->GetSelection( indices, count );

		if (indices && count)
		{
			MSG_FolderInfo *folderInfo = MSG_GetFolderInfo( pFolderPane, indices[0] );
			MSG_GetFolderLineByIndex( pFolderPane, indices[0], 1, &folderLine );

			CString csTitle;
			if (folderLine.flags & (MSG_FOLDER_FLAG_NEWSGROUP|MSG_FOLDER_FLAG_CATEGORY)) {
				pCmdUI->SetText(szLoadString(IDS_POPUP_NEWSGROUPPROP));
			} else if (folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST) {
				pCmdUI->SetText(szLoadString(IDS_POPUP_NEWSHOSTPROP));
			} else {
				if (folderLine.level > 1) {
					pCmdUI->SetText(szLoadString(IDS_POPUP_FOLDERPROP));
				}else {
					pCmdUI->SetText(szLoadString(IDS_POPUP_MAILSERVERPROP));
				}
			}

		}
		pCmdUI->Enable( count == 1 );
	}
}


