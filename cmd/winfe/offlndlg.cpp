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

#include "stdafx.h"
#include "mailpriv.h"
#include "wfemsg.h"
#include "prefapi.h"
#include "nethelp.h"
#include "xp_help.h"
#include "offpkdlg.h"
#include "offlndlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COfflineInfo
// Note bGoOffline is true if we need to go offline, false if we need
// to go online
// bChangeState is TRUE if the bGoOffline parameter is to be used,
// FALSE if it will be ignored.
COfflineInfo::COfflineInfo(BOOL bDownloadMail, BOOL bDownloadNews,
						   BOOL bDownloadDirectories,
						   BOOL bSendMail, BOOL bDownloadFlagged,
						   BOOL bGoOffline, BOOL bChangeState)
{
	m_bDownloadMail = bDownloadMail;
	m_bDownloadNews = bDownloadNews;
	m_bDownloadDirectories = bDownloadDirectories;
	m_bSendMail = bSendMail;
	m_bDownloadFlagged = bDownloadFlagged;
	m_bGoOffline = bGoOffline;
	m_bChangeState = bChangeState;
}

BOOL COfflineInfo::DownloadMail()
{
	return m_bDownloadMail;
}

BOOL COfflineInfo::DownloadNews()
{
	return m_bDownloadNews;
}

BOOL COfflineInfo::DownloadDirectories()
{
	return m_bDownloadDirectories;
}

BOOL COfflineInfo::SendMail()
{
	return m_bSendMail;
}

BOOL COfflineInfo::DownloadFlagged()
{
	return m_bDownloadFlagged;
}

BOOL COfflineInfo::GoOffline()
{
	return m_bGoOffline;
}

BOOL COfflineInfo::ChangeState()
{
	return m_bChangeState;
}
/////////////////////////////////////////////////////////////////////////////
// COfflineDlg dialog


COfflineDlg::COfflineDlg(CWnd* pParent /*=NULL*/)
	: CDialog(COfflineDlg::IDD, pParent)
{
	XP_Bool bDiscussions = 0;
	XP_Bool bMail        = 0;
	XP_Bool bMessages    = 0;
	XP_Bool bDirectories = 0;
	XP_Bool bFlagged	 = 0;
	XP_Bool bGoOffline   = 0;

	PREF_GetBoolPref("offline.download_discussions",&bDiscussions);
	PREF_GetBoolPref("offline.download_mail",&bMail);
	PREF_GetBoolPref("offline.download_messages",&bMessages);
	PREF_GetBoolPref("offline.download_directories",&bDirectories);
//	PREF_GetBoolPref("offline.download_flagged", &bFlagged);
	PREF_GetBoolPref("offline.download_gooffline",&bGoOffline);



	m_bDownLoadDiscussions = bDiscussions;
	m_bDownLoadMail        = bMail;
	m_bSendMessages        = bMessages;
	m_bDownLoadDirectories = bDirectories;
//	m_bDownLoadFlagged     = bFlagged;
	m_bGoOffline           = bGoOffline;

	m_bMode = NET_IsOffline();
}

void COfflineDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_DWNLD_CHECK_MAIL,          m_bDownLoadMail);
	DDX_Check(pDX, IDC_DWNLD_CHECK_DISCUSSIONS,   m_bDownLoadDiscussions);
	DDX_Check(pDX, IDC_DWNLD_CHECK_DIRECTORIES,   m_bDownLoadDirectories);
	DDX_Check(pDX, IDC_DWNLD_CHECK_SENDMAIL,      m_bSendMessages);
//	DDX_Check(pDX, IDC_DOWNLOADFLAGGED,			  m_bDownLoadFlagged);
	DDX_Check(pDX, IDC_DWNLD_CHECK_GO_OFFLINE,    m_bGoOffline);

}


BEGIN_MESSAGE_MAP(COfflineDlg, CDialog)
	//{{AFX_MSG_MAP(COfflineDlg)
	ON_BN_CLICKED(IDC_DWNLD_BUTTON_SELECT, OnButtonSelect)
	ON_BN_CLICKED(IDC_DWNLD_HELP_GO_OFFLINE, OnHelp)
	ON_WM_PAINT()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COfflineDlg message handlers

void COfflineDlg::OnButtonSelect() 
{
	CDlgOfflinePicker OfflinePickerDlg(this);
	OfflinePickerDlg.DoModal();
	/*
	CDlgSelectGroups dlgSelectGroups(this);
	dlgSelectGroups.DoModal();
	CWnd *pWnd = GetDlgItem(IDC_TEXT_DISCUSSIONS_SELECTED);
	CString strCountText;
	CString strTemp;

	if (pWnd)
	{
		strTemp.LoadString(IDS_TEXT_DISCUSSIONS_SELECTED);
		char buf[20];
		strCountText = itoa(dlgSelectGroups.GetSelectionCount(),buf,10) + (CString)" "+ strTemp;
		pWnd->SetWindowText(strCountText);	
	}
	
	pWnd = GetDlgItem(IDC_TEXT_FOLDERS_SELECTED);
	if (pWnd)
	{
		strTemp.LoadString(IDS_TEXT_FOLDERS_SELECTED);
		char buf[20];
		strCountText = itoa(dlgSelectGroups.GetMailSelectionCount(),buf,10) + (CString)" "+ strTemp;
		pWnd->SetWindowText(strCountText);	
	}
	*/
}

void COfflineDlg::ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure)
{
	if (::IsWindow(hwnd)) {
		::ShowWindow(hwnd,SW_SHOW);
		::UpdateWindow(hwnd);
	}

	if (pane)
	{
		COfflineInfo *info = (COfflineInfo *) closure;


		BOOL bGoOffline = info->ChangeState() && info->GoOffline();
		MSG_SynchronizeOffline(WFE_MSGGetMaster(), pane, info->DownloadNews(),
							   info->DownloadMail(), info->SendMail(), info->DownloadDirectories(),
							   bGoOffline);

	}
}

BOOL COfflineDlg::ShowOnlineCallBack(HWND hWnd, MSG_Pane *pane, void * closure)
{
	BOOL bSendMail = FALSE;
	if (pane)
	{
		COfflineInfo *info = (COfflineInfo *) closure;

		bSendMail = info->SendMail();

		//handle going online:
		if(info->ChangeState() && !info->GoOffline())
		{

			XP_Bool bSelectable;
			MSG_COMMAND_CHECK_STATE selected;
			const char * pString;
			XP_Bool bPlural;

			MSG_CommandStatus (pane,MSG_DeliverQueuedMessages,
							  NULL, 0,&bSelectable, &selected, &pString, 
							  &bPlural);
			int32 prefInt;


			PREF_GetIntPref("offline.send.unsent_messages", &prefInt);

			//check for unsent messages.
			if(prefInt == 0)
			{
				bSendMail = FALSE;
				if(bSelectable)
				{
					int result = ::MessageBox(hWnd, ::szLoadString(IDS_SENDONLINE),
											  ::szLoadString(IDS_GOONLINE), 
											  MB_YESNO | MB_APPLMODAL);

					bSendMail = (result == IDYES);
				}

			}
			else if (prefInt == 1)
			{
				//if there's mail send it otherwise don't.
				bSendMail = bSelectable;
			}
			else
			{
				bSendMail = FALSE;
			}
		}


	}
	//if bSendMail is true we want to bring up a progress pane
	return bSendMail;
}

void WFE_Synchronize(CWnd *pParent, BOOL bExitAfterSynchronizing)
{
	if(bExitAfterSynchronizing)
	{
		theApp.HideFrames();
	}

	BOOL bOnline;
    COfflineDlg rDlg(pParent);
    PREF_GetBoolPref("network.online", &bOnline);
    rDlg.InitDialog(!bOnline);
    int result = rDlg.DoModal();

	if(result == IDOK && rDlg.DownloadItems())
	{
		//note we only change state if we go offline.
		COfflineInfo info(rDlg.m_bDownLoadMail, rDlg.m_bDownLoadDiscussions,
						  rDlg.m_bDownLoadDirectories,
						  rDlg.m_bSendMessages, FALSE,
						  rDlg.m_bGoOffline, rDlg.m_bGoOffline);

		theApp.m_bSynchronizing = TRUE;
		new COfflineProgressDialog(pParent, NULL,COfflineDlg::ShutDownFrameCallBack,
			  &info,szLoadString(IDS_SYNCHRONIZING), NULL, bExitAfterSynchronizing);
	}
	else
	{
		if(bExitAfterSynchronizing)
		{
			theApp.CommonAppExit();
		}
	}
}

/* rhp - This is for doing MAPI Synchronizing... */
void 
WFE_MAPISynchronize(CWnd *pParent)
{
  BOOL bOnline;
  COfflineDlg rDlg(pParent);
  PREF_GetBoolPref("network.online", &bOnline);
  rDlg.InitDialog(!bOnline);
  // int result = rDlg.DoModal(); - no need for showing dialog...

  // we only change state if we go offline.
  COfflineInfo info(rDlg.m_bDownLoadMail, rDlg.m_bDownLoadDiscussions,
    rDlg.m_bDownLoadDirectories,
    rDlg.m_bSendMessages, FALSE,
    rDlg.m_bGoOffline, rDlg.m_bGoOffline);

  theApp.m_bSynchronizing = TRUE;
  new COfflineProgressDialog(pParent, NULL,COfflineDlg::ShutDownFrameCallBack,
    &info,szLoadString(IDS_SYNCHRONIZING), NULL, FALSE);
}

/*
 *  Return if there are items to download based on this dialog's current settings
 */
BOOL COfflineDlg::DownloadItems()
{

	return((m_bDownLoadDiscussions || m_bDownLoadMail || m_bSendMessages ||
		 m_bDownLoadDirectories ));

}

void COfflineDlg::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData();

/*	if ((m_bDownLoadDiscussions || m_bDownLoadMail || m_bSendMessages ||
		 m_bDownLoadDirectories ))
	{
	      new CProgressDialog(this, NULL,ShutDownFrameCallBack,
            this,szLoadString(IDS_DOWNLOADINGARTICLES));	// need correct window title
			;//DownLoad!!!!!!!
	}
	else
	{
		PREF_SetBoolPref("network.online", m_bMode);
	}
*/
	if (!m_bDownLoadDiscussions && !m_bDownLoadMail && !m_bDownLoadDirectories && !m_bSendMessages)
		PREF_SetBoolPref("network.online", !m_bGoOffline);

	PREF_SetBoolPref("offline.download_discussions",(XP_Bool)m_bDownLoadDiscussions);
	PREF_SetBoolPref("offline.download_mail",(XP_Bool)m_bDownLoadMail);
	PREF_SetBoolPref("offline.download_directories",(XP_Bool)m_bDownLoadDirectories);
	PREF_SetBoolPref("offline.download_messages",(XP_Bool)m_bSendMessages);
//	PREF_SetBoolPref("offline.download_flagged", (XP_Bool)m_bDownLoadFlagged);

	CDialog::OnOK();
}

void COfflineDlg::OnHelp()
{
	NetHelp(HELP_MAILNEWS_SYNCHRONIZE);
}

int COfflineDlg::DoModal()
{
	if (!m_MNResourceSwitcher.Initialize())
		return -1;
	return CDialog::DoModal();
}

BOOL COfflineDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_MNResourceSwitcher.Reset();

	CWnd *pWnd1 = GetDlgItem(IDC_DWNLD_STATIC_MGD);
	if (!pWnd1 )
		return FALSE;

	HFONT hFont = (HFONT)this->SendMessage(WM_GETFONT);
	if (hFont != NULL)
	{   //make the title bold
		VERIFY(::GetObject(hFont, sizeof(LOGFONT), &m_LogFont));
		m_LogFont.lfWeight=FW_BOLD;
		m_Font.CreateFontIndirect(&m_LogFont);

		pWnd1->SetFont(&m_Font);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
}



void COfflineDlg::OnDestroy( )
{
	int i = 0;
}



///////////////////////////////////////////////////////////////////////
////CGoOfflineAndSynchDlg

CGoOfflineAndSynchDlg::CGoOfflineAndSynchDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CGoOfflineAndSynchDlg::IDD, pParent)
{
	XP_Bool bDiscussions = 0;
	XP_Bool bMail        = 0;
	XP_Bool bMessages    = 0;
#ifdef WIN32
	XP_Bool bPages       = 0;
	XP_Bool bChannels    = 0;
#endif
	XP_Bool bDirectories = 0;
	XP_Bool bGoOffline   = 0;

	PREF_GetBoolPref("offline.download_discussions",&bDiscussions);
	PREF_GetBoolPref("offline.download_mail",&bMail);
	PREF_GetBoolPref("offline.download_messages",&bMessages);

#ifdef WIN32
	PREF_GetBoolPref("offline.download_pages",&bPages);
	PREF_GetBoolPref("offline.download_channels",&bChannels);
#endif
	PREF_GetBoolPref("offline.download_directories",&bDirectories);
	PREF_GetBoolPref("offline.download_gooffline",&bGoOffline);

	m_bDownLoadDiscussions = bDiscussions;
	m_bDownLoadMail        = bMail;
	m_bSendMessages        = bMessages;

#ifdef WIN32
	m_bDownLoadPages       = bPages;
	m_bDownLoadChannels    = bChannels;
#endif
	m_bDownLoadDirectories = bDirectories;
	m_bGoOffline           = bGoOffline;

	m_bMode = NET_IsOffline();//get offline mode
}

BEGIN_MESSAGE_MAP(CGoOfflineAndSynchDlg, CDialog)
	//{{AFX_MSG_MAP(CGoOfflineAndSynchDlg)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
	ON_BN_CLICKED(IDC_HELP_DOWNLOAD, OnHelp)
	ON_BN_CLICKED(IDC_BUTTON_NO, OnNo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COfflineDlg message handlers

void CGoOfflineAndSynchDlg::OnCancel() 
{
	CDialog::OnCancel();
}

void CGoOfflineAndSynchDlg::OnNo()
{
	PREF_SetBoolPref("network.online", m_bMode);
	CDialog::OnOK();
}

void CGoOfflineAndSynchDlg::ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure)
{
	if (::IsWindow(hwnd)) {
		::ShowWindow(hwnd,SW_SHOW);
		::UpdateWindow(hwnd);
	}

	if (pane)
	{
		CGoOfflineAndSynchDlg *SynchronizeDlg = (CGoOfflineAndSynchDlg *) closure;
		MSG_GoOffline(WFE_MSGGetMaster(), pane, SynchronizeDlg->m_bDownLoadDiscussions, SynchronizeDlg->m_bDownLoadMail,
			SynchronizeDlg->m_bSendMessages, FALSE /*getDirectories*/);
	}
}

void CGoOfflineAndSynchDlg::OnOK() 
{
	// TODO: Add extra validation here
#ifdef WIN32
	if ((m_bDownLoadDiscussions || m_bDownLoadMail || m_bSendMessages ||
		 m_bDownLoadDirectories || m_bDownLoadPages || m_bDownLoadChannels))
#else
	if ((m_bDownLoadDiscussions || m_bDownLoadMail || m_bSendMessages ||
		 m_bDownLoadDirectories ))
#endif
	{
	      new CProgressDialog(this, NULL,ShutDownFrameCallBack,
            this,szLoadString(IDS_DOWNLOADINGARTICLES));	// need correct window title
			;//DownLoad!!!!!!!
			// somehow we need to wait until downloading is done.
	}
	else
	{
		PREF_SetBoolPref("network.online", m_bMode);
	}

	PREF_SetBoolPref("offline.download_discussions",(XP_Bool)m_bDownLoadDiscussions);
	PREF_SetBoolPref("offline.download_mail",(XP_Bool)m_bDownLoadMail);
	PREF_SetBoolPref("offline.download_directories",(XP_Bool)m_bDownLoadDirectories);
	PREF_SetBoolPref("offline.download_messages",(XP_Bool)m_bSendMessages);

	CDialog::OnOK();
}

void CGoOfflineAndSynchDlg::OnHelp()
{
	NetHelp(HELP_OFFLINE_DOWNLOAD);
}


int CGoOfflineAndSynchDlg::DoModal()
{
	if (!m_MNResourceSwitcher.Initialize())
		return -1;
	return CDialog::DoModal();
}

BOOL CGoOfflineAndSynchDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_MNResourceSwitcher.Reset();

	CWnd *pWnd1 = GetDlgItem(IDC_STATIC_SYNC_QUESTION);
	if (!pWnd1)
		return FALSE;

	HFONT hFont = (HFONT)this->SendMessage(WM_GETFONT);
	if (hFont != NULL)
	{   //make the title bold
		VERIFY(::GetObject(hFont, sizeof(LOGFONT), &m_LogFont));
		m_LogFont.lfWeight=FW_BOLD;
		m_Font.CreateFontIndirect(&m_LogFont);
		pWnd1->SetFont(&m_Font);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
}

/////////////////////////////////////////////////////////////////////////////
// CAskSynchronizeExitDlg dialog


CAskSynchronizeExitDlg::CAskSynchronizeExitDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAskSynchronizeExitDlg::IDD, pParent)
{

}

void CAskSynchronizeExitDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_DONTASKAGAIN,          m_bDontAskAgain);

}


BEGIN_MESSAGE_MAP(CAskSynchronizeExitDlg, CDialog)
	//{{AFX_MSG_MAP(CAskSynchronizeExitDlg)
	ON_BN_CLICKED(IDYES, OnYes)
	ON_BN_CLICKED(IDNO, OnNo)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAskSynchronizeExitDlg message handlers



void CAskSynchronizeExitDlg::OnYes() 
{
	CommonClose();
	EndDialog(IDYES);
}

void CAskSynchronizeExitDlg::OnNo()
{
	CommonClose();
	EndDialog(IDNO);
}

void CAskSynchronizeExitDlg::CommonClose()
{
	// TODO: Add extra validation here
	UpdateData();


	if(m_bDontAskAgain)
	{
		PREF_SetBoolPref("offline.prompt_synch_on_exit",(XP_Bool)!m_bDontAskAgain);
	}
}


