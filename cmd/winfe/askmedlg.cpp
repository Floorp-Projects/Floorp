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

// AskMeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "askmedlg.h"
#include "prefapi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


void AskMeDlg(void)
{
	    int32 nPromptAskMe = FALSE;
	    PREF_GetIntPref("offline.startup_state", &nPromptAskMe);
		if(nPromptAskMe == 0) //old setting (note, network.online gets initialized before
							  //user prefs read in so we have to reset this now
		{
			XP_Bool boolPref = FALSE;
			PREF_GetBoolPref("network.online", &boolPref);
			PREF_SetBoolPref("network.online", !boolPref);
			PREF_SetBoolPref("network.online", boolPref);

		}
		else if (nPromptAskMe==1)            //askme
	    {
		    CAskMeDlg rAskMeDlg(nPromptAskMe);
		    rAskMeDlg.DoModal();
	    }
}

/////////////////////////////////////////////////////////////////////////////
// CAskMeDlg dialog


CAskMeDlg::CAskMeDlg(BOOL bDefault/*FALSE*/, int nOnOffLine/*0-the default*/,
					 CWnd* pParent /*=NULL*/)
	: CDialog(CAskMeDlg::IDD, pParent)
{
	XP_Bool bOnline = TRUE;
	PREF_GetBoolPref("network.online", &bOnline);

	//{{AFX_DATA_INIT(CAskMeDlg)
	m_nStartupSelection = (bOnline == 1 ? 0 : 1);
	m_bAskMeDefault = 0;
	//}}AFX_DATA_INIT
}


void CAskMeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAskMeDlg)
	DDX_Radio(pDX, IDC_RADIO_ONLINE, m_nStartupSelection);
	DDX_Check(pDX, IDC_CHECK_ASKME_DEFAULT, m_bAskMeDefault);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAskMeDlg, CDialog)
	//{{AFX_MSG_MAP(CAskMeDlg)
	ON_BN_CLICKED(IDC_CHECK_ASKME_DEFAULT, OnCheckAskMeDefault)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAskMeDlg message handlers

void CAskMeDlg::OnOK() 
{
    //Save any preference settings that may have been chosen
	CDialog::OnOK();
	UpdateData();
	if (IsDlgButtonChecked(IDC_CHECK_ASKME_DEFAULT))
	{
		//remember previous setting the next time we exit.
		PREF_SetIntPref("offline.startup_state",0);
	}

	if (IsDlgButtonChecked(IDC_RADIO_ONLINE))
		PREF_SetBoolPref("network.online", TRUE);
	else  
		PREF_SetBoolPref("network.online", FALSE);
}

void CAskMeDlg::OnCheckAskMeDefault() 
{   
}


void CAskMeDlg::EnableDisableItem(BOOL bState, UINT nIDC)
{
	CWnd *pWnd = GetDlgItem(nIDC);
	if (pWnd)
	{
		pWnd->EnableWindow(bState);
	}
}

BOOL CAskMeDlg::OnInitDialog() 
{
	BOOL bReturn = CDialog::OnInitDialog();
	CWnd *pWnd = NULL;

	HFONT hFont = (HFONT)this->SendMessage(WM_GETFONT);
	if (hFont != NULL)
	{   //make the title bold
		VERIFY(::GetObject(hFont, sizeof(LOGFONT), &m_LogFont));
		m_LogFont.lfWeight=FW_BOLD;
		m_hFont = theApp.CreateAppFont( m_LogFont );
		::SendMessage(::GetDlgItem(m_hWnd, IDC_STATIC_TITLE), WM_SETFONT, (WPARAM)m_hFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_RADIO_ONLINE), WM_SETFONT, (WPARAM)m_hFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_RADIO_OFFLINE), WM_SETFONT, (WPARAM)m_hFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_CHECK_ASKME_DEFAULT), WM_SETFONT, (WPARAM)m_hFont, FALSE);
	}

	return bReturn; 
}
