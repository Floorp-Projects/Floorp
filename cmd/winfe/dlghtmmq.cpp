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

// dlghtmmq.cpp : implementation file
//

#include "stdafx.h"
#include "msgcom.h"
#include "dlghtmmq.h"
#include "mailmisc.h"
#include "nethelp.h"
#include "xp_help.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CHtmlMailQuestionDlg dialog


CHtmlMailQuestionDlg::CHtmlMailQuestionDlg(MSG_Pane* pComposePane, CWnd* pParent /*=NULL*/)
	: CDialog(CHtmlMailQuestionDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CHtmlMailQuestionDlg)
	m_nRadioValue = -1;
	//}}AFX_DATA_INIT
	m_pComposePane = pComposePane;
}


void CHtmlMailQuestionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHtmlMailQuestionDlg)
	DDX_Radio(pDX, IDC_RADIO1, m_nRadioValue);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CHtmlMailQuestionDlg, CDialog)
	//{{AFX_MSG_MAP(CHtmlMailQuestionDlg)
	ON_BN_CLICKED(IDC_BTN_RECIPIENTS, OnBtnRecipients)
	ON_BN_CLICKED(IDC_HELP_HTMLOK, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHtmlMailQuestionDlg message handlers

BOOL CHtmlMailQuestionDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_HTMLComposeAction = MSG_GetHTMLAction(m_pComposePane);

	switch(m_HTMLComposeAction)
	{
		case MSG_HTMLUseMultipartAlternative:
			m_nRadioValue = 0;
			break;

		case MSG_HTMLConvertToPlaintext:
			m_nRadioValue = 1;
			break;

		case MSG_HTMLSendAsHTML:
			m_nRadioValue =2; 
			break;

		default://give them the choice to send both up front.
			m_nRadioValue = 0;

	}
	
	UpdateData(FALSE);
	return TRUE;  

}

void CHtmlMailQuestionDlg::OnBtnRecipients() 
{
	MSG_PutUpRecipientsDialog(m_pComposePane,this);
}

void CHtmlMailQuestionDlg::OnOK() 
{
	UpdateData();
	//assign the new setting
	switch(m_nRadioValue)
	{
		case 0:
			m_HTMLComposeAction = MSG_HTMLUseMultipartAlternative;
			break;

		case 1:
			m_HTMLComposeAction = MSG_HTMLConvertToPlaintext;
			break;

		case 2:
			m_HTMLComposeAction = MSG_HTMLSendAsHTML;
			break;

		default:
			m_HTMLComposeAction = MSG_HTMLUseMultipartAlternative;
	}

	MSG_SetHTMLAction(m_pComposePane, m_HTMLComposeAction);

	CDialog::OnOK();
}

void CHtmlMailQuestionDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();

}


void CHtmlMailQuestionDlg::OnHelp() 
{
	NetHelp(HELP_HTML_MAIL_QUESTION);
}


int CreateAskHTMLDialog (MSG_Pane* composepane, void* closure)
{
	// TODO: Add your control notification handler code here
	if (!composepane) 
		return 0;
	CWnd *pWnd = NULL;

	pWnd = (CWnd*)MSG_GetFEData( composepane );
	CHtmlMailQuestionDlg rHtmlMQDlg(composepane,pWnd);

	int ret = rHtmlMQDlg.DoModal();
	
	if (ret == IDOK)
	{
		//do this only if the user clicked the Send button
		return(0);		// return success
	}			  
	
	return 1;		// cancel
}

