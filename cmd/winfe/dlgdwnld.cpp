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

// dlgdwnld.cpp : implementation file
//
#include "stdafx.h"
#include "dlgdwnld.h"
#include "dlgseldg.h"
#include "mailpriv.h"
#include "wfemsg.h"
#include "prefapi.h"
#include "nethelp.h"
#include "xp_help.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDownLoadDlg dialog


CDownLoadDlg::CDownLoadDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDownLoadDlg::IDD, pParent)
{
	XP_Bool bDiscussions = 0;
	XP_Bool bMail = 0;
	XP_Bool bMessages = 0;

	PREF_GetBoolPref("offline.download_discussions",&bDiscussions);
	PREF_GetBoolPref("offline.download_mail",&bMail);
	PREF_GetBoolPref("offline.download_messages",&bMessages);

	//{{AFX_DATA_INIT(CDownLoadDlg)
	m_bDownLoadDiscusions = bDiscussions;
	m_bDownLoadMail = bMail;
	m_bSendMessages = bMessages;
	//}}AFX_DATA_INIT
	m_bMode = NET_IsOffline();
}

void CDownLoadDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDownLoadDlg)
	DDX_Check(pDX, IDC_CHECK_DOWNLOAD_DISC, m_bDownLoadDiscusions);
	DDX_Check(pDX, IDC_CHECK_DOWNLOAD_MAIL, m_bDownLoadMail);
	DDX_Check(pDX, IDC_CHECK_SEND_MESSAGES, m_bSendMessages);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDownLoadDlg, CDialog)
	//{{AFX_MSG_MAP(CDownLoadDlg)
	ON_BN_CLICKED(IDC_BUTTON_SELECT, OnButtonSelect)
	ON_BN_CLICKED(IDC_HELP_GO_OFFLINE, OnHelp)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDownLoadDlg message handlers

void CDownLoadDlg::OnButtonSelect() 
{
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
}

void CDownLoadDlg::ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure)
{
	if (::IsWindow(hwnd)) {
		::ShowWindow(hwnd,SW_SHOW);
		::UpdateWindow(hwnd);
	}

	if (pane)
	{
		CDownLoadDlg *downloadDlg = (CDownLoadDlg *) closure;
		MSG_GoOffline(WFE_MSGGetMaster(), pane, downloadDlg->m_bDownLoadDiscusions, downloadDlg->m_bDownLoadMail,
			downloadDlg->m_bSendMessages);
	}
}

void CDownLoadDlg::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData();

	if ((m_bDownLoadDiscusions || m_bDownLoadMail || m_bSendMessages))
	{
	      new CProgressDialog(NULL, NULL,ShutDownFrameCallBack,
            this,szLoadString(IDS_DOWNLOADINGARTICLES));	// need correct window title
			;//DownLoad!!!!!!!

	}
	else
		PREF_SetBoolPref("network.online", m_bMode);


	PREF_SetBoolPref("offline.download_discussions",(XP_Bool)m_bDownLoadDiscusions);
	PREF_SetBoolPref("offline.download_mail",(XP_Bool)m_bDownLoadMail);
	PREF_SetBoolPref("offline.download_messages",(XP_Bool)m_bSendMessages);


	CDialog::OnOK();
}

void CDownLoadDlg::OnHelp()
{
	NetHelp(HELP_OFFLINE_DOWNLOAD);
}

void CDownLoadDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	CBitmap *poldbmp,*poldbmp2,*poldbmp3;
	CBitmap m_bmp1,m_bmp2,m_bmp3;
	CDC m_memdc,m_memdc2,m_memdc3;
	CWnd *pWnd = NULL;
	CRect rect(0,0,0,0);

	
	//---------------------------------------------------
	//BITMAP #1
	pWnd = GetDlgItem(IDC_CHECK_DOWNLOAD_MAIL);
	if (pWnd)
	{
		pWnd->GetWindowRect(&rect);
		ScreenToClient(&rect);
	}
	// Load the bitmap resources
	m_bmp1.LoadBitmap( IDB_MAIL );
	// Create a compatible memory DC
	m_memdc.CreateCompatibleDC( &dc );
	// Select the bitmap into the DC
	poldbmp = m_memdc.SelectObject( &m_bmp1 );
	// Copy (BitBlt) bitmap from memory DC to screen DC
	dc.BitBlt( rect.left -28 ,rect.top, 18, 17, &m_memdc, 0, 0, SRCCOPY );
	m_memdc.SelectObject( poldbmp );

	//---------------------------------------------------
	//BITMAP #2
	pWnd = GetDlgItem(IDC_CHECK_DOWNLOAD_DISC);
	if (pWnd)
	{
		pWnd->GetWindowRect(&rect);
		ScreenToClient(&rect);
	}

	m_bmp2.LoadBitmap( IDB_DISCUSSIONS_1 );
	// Create a compatible memory DC
	m_memdc2.CreateCompatibleDC( &dc );
	// Select the bitmap into the DC
	poldbmp2 = m_memdc2.SelectObject( &m_bmp2 );
	// Copy (BitBlt) bitmap from memory DC to screen DC
	dc.BitBlt( rect.left -28, rect.top, 17, 16, &m_memdc2, 0, 0, SRCCOPY );
	m_memdc2.SelectObject( poldbmp2 );

	//---------------------------------------------------
	//BITMAP #3
	pWnd = GetDlgItem(IDC_CHECK_SEND_MESSAGES);
	if (pWnd)
	{
		pWnd->GetWindowRect(&rect);
		ScreenToClient(&rect);
	}
	m_bmp3.LoadBitmap( IDB_MESSAGE );
	// Create a compatible memory DC
	m_memdc3.CreateCompatibleDC( &dc );
	// Select the bitmap into the DC
	poldbmp3 = m_memdc3.SelectObject( &m_bmp3 );
	// Copy (BitBlt) bitmap from memory DC to screen DC
	dc.BitBlt( rect.left -28, rect.top, 19, 14, &m_memdc3, 0, 0, SRCCOPY );
	m_memdc3.SelectObject( poldbmp3 );
}


BOOL CDownLoadDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CWnd *pWnd1 = GetDlgItem(IDC_TEXT_ACTION);
	CWnd *pWnd2 = GetDlgItem(IDC_TEXT_ACTION2);
	CWnd *pWnd3 = GetDlgItem(IDOK);
	if (!pWnd1 || !pWnd2 || !pWnd3)
		return FALSE;

	CString strText;
	if (m_bMode)
	{   //We are going Online!!
		strText.LoadString(IDS_GO_ONLINE_NOW);
		pWnd1->SetWindowText(strText);
		strText.LoadString(IDS_ACTION_ONLINE);
		pWnd2->SetWindowText(strText);
		strText.LoadString(IDS_BUTTON_TEXT_CONNECT);
		pWnd3->SetWindowText(strText);
		
	}
	else
	{   //We are going Offline!!
		strText.LoadString(IDS_GO_OFFLINE_NOW);
		pWnd1->SetWindowText(strText);
		strText.LoadString(IDS_ACTION_OFFLINE);
		pWnd2->SetWindowText(strText);
		strText.LoadString(IDS_BUTTON_TEXT_DISCONNECT);
		pWnd3->SetWindowText(strText);		
	}

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

