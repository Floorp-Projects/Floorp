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
#include "setupwiz.h"
#include "mucproc.h"
#include "logindg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static  CMucProc m_pMucProc;

/////////////////////////////////////////////////////////////////////////////
// CMucIntroPage dialog

#ifdef XP_WIN32
CMucIntroPage::CMucIntroPage(CWnd* pParent)
	: CNetscapePropertyPage(IDD)
#else
CMucIntroPage::CMucIntroPage(CWnd* pParent)
	: CDialog()
#endif  
{
	m_pParent = (CNewProfileWizard*)pParent;
	m_pParent->m_bASWEnabled = FALSE;
	m_pParent->m_bMucEnabled = TRUE;
}

#ifdef XP_WIN32
BOOL CMucIntroPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_NEXT | PSWIZB_BACK);
    return CNetscapePropertyPage::OnSetActive();
}
#else
BOOL CMucIntroPage::Create(UINT nID, CWnd *pWnd)
{       
	return CDialog::Create(nID, pWnd);      
}
#endif

void CMucIntroPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


#ifdef XP_WIN32
BEGIN_MESSAGE_MAP(CMucIntroPage, CNetscapePropertyPage)
#else
BEGIN_MESSAGE_MAP(CMucIntroPage, CDialog)
#endif
	//{{AFX_MSG_MAP(CMucIntroPage)
	ON_BN_CLICKED(IDC_MUCINTRO_ACCT_EXIST, OnMucIntroAcctExist)
	ON_BN_CLICKED(IDC_MUCINTRO_ACCT_SYS, OnMucIntroAcctSys)
	ON_BN_CLICKED(IDC_MUCINTRO_ACCT_ADD, OnMucIntroAcctAdd)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMucIntroPage message handlers

BOOL CMucIntroPage::OnInitDialog() 
{                                                                                      

	if(m_pParent->m_bUpgrade)
	{
		((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_EXIST)) -> SetCheck(TRUE);
		((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_SYS)) -> SetCheck(FALSE);
		((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_ADD)) -> SetCheck(FALSE);
		GetDlgItem(IDC_MUCINTRO_ACCT_SYS) -> EnableWindow(FALSE);
		GetDlgItem(IDC_MUCINTRO_ACCT_ADD) -> EnableWindow(FALSE);
		GetDlgItem(IDC_ACCTSYS_TEXT) -> EnableWindow(FALSE);
		GetDlgItem(IDC_ACCTADD_TEXT) -> EnableWindow(FALSE);

		MucIntroProc(m_OptExist);
	}
	else
	{
		((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_EXIST)) -> SetCheck(FALSE);
		((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_SYS)) -> SetCheck(FALSE);
		((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_ADD)) -> SetCheck(TRUE);

		MucIntroProc(m_OptAdd);
	}
#ifdef XP_WIN32
	return CNetscapePropertyPage::OnInitDialog();
#else
	RECT    rect;
	m_pParent->GetWindowRect(&rect);
	m_height = rect.bottom-rect.top-80;
	m_width = rect.right-rect.left-10;
		
	SetWindowPos(&wndTop, rect.left, rect.top+30, m_width, m_height, SWP_HIDEWINDOW);
	return TRUE;
#endif
}

void CMucIntroPage::OnMucIntroAcctAdd() 
{
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_ADD)) -> SetCheck(TRUE);
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_EXIST)) -> SetCheck(FALSE);
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_SYS)) -> SetCheck(FALSE);

	MucIntroProc(m_OptAdd);
}

void CMucIntroPage::OnMucIntroAcctExist() 
{
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_ADD)) -> SetCheck(FALSE);
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_EXIST)) -> SetCheck(TRUE);
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_SYS)) -> SetCheck(FALSE);

	MucIntroProc(m_OptExist);
}

void CMucIntroPage::OnMucIntroAcctSys() 
{
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_ADD)) -> SetCheck(FALSE);
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_EXIST)) -> SetCheck(FALSE);
	((CButton *)GetDlgItem(IDC_MUCINTRO_ACCT_SYS)) -> SetCheck(TRUE);

	MucIntroProc(m_OptSys);
}

void CMucIntroPage::MucIntroProc(int m_option) 
{
	switch(m_option)
	{
		case m_OptAdd:
			m_pParent->m_bASWEnabled = TRUE;
			m_pParent->m_bMucEnabled = FALSE;
		break;

		case m_OptExist:        // enable MUC
			m_pParent->m_bASWEnabled = FALSE;
			if(!m_pMucProc.LoadMuc())
			{
				AfxMessageBox("Muc.dll is missing");
				m_pParent->m_bMucEnabled = FALSE;
			}
			else
				m_pParent->m_bMucEnabled = TRUE;
		break;

		case m_OptSys:  // disable MUC  
			m_pParent->m_bASWEnabled = FALSE;
			m_pParent->m_bMucEnabled = FALSE;
		break;

		default:
			break;
	}
}
void CMucIntroPage::SetMove(int x, int y, int nShowCmd)
{
	SetWindowPos(&wndTop, x, y, m_width, m_height, nShowCmd);
}       

/////////////////////////////////////////////////////////////////////////////
// CMucEditPage dialog

#ifdef XP_WIN32
CMucEditPage::CMucEditPage(CWnd* pParent, BOOL bEditView)
	: CNetscapePropertyPage(IDD)
#else
CMucEditPage::CMucEditPage(CWnd* pParent,BOOL bEditView)
	: CDialog()
#endif
{
	//{{AFX_DATA_INIT(CMucEditPage)
	//}}AFX_DATA_INIT
	
	m_bEditView = bEditView;

	if(m_bEditView)
	{
		m_pEditParent = (CNewProfileWizard*)pParent;
		m_pEditParent->m_pAcctName = "";
		m_pEditParent->m_pModemName = "";
	}
	else
		m_pViewParent = (CMucViewWizard*)pParent;

	m_acctSelect = "";
	m_modemSelect = "";
}

CMucEditPage::~CMucEditPage()
{
}

#ifdef XP_WIN32
BOOL CMucEditPage::OnSetActive()
{
	CString	m_str;

	if(m_bEditView)
	{
		if (m_pEditParent->m_bUpgrade) 
		{
			m_str.LoadString(IDS_PEMUCEDIT_UPGRADE);
			m_pEditParent->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
		}
		else
		{
			m_str.LoadString(IDS_PEMUCEDIT_NORMAL);
			m_pEditParent->SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);
		}
		GetDlgItem(IDC_EDIT_TEXT)->SetWindowText(m_str);
	}
	else
		m_pViewParent->SetWizardButtons(PSWIZB_FINISH);

    return CNetscapePropertyPage::OnSetActive();
}
#else
BOOL CMucEditPage::Create(UINT nID, CWnd *pWnd)
{       
	return CDialog::Create(nID, pWnd);      
}
#endif

void CMucEditPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

#ifdef XP_WIN32
BEGIN_MESSAGE_MAP(CMucEditPage, CNetscapePropertyPage)
#else
BEGIN_MESSAGE_MAP(CMucEditPage, CDialog)
#endif
	//{{AFX_MSG_MAP(CMucEditPage)
//      ON_CBN_SELENDOK(IDC_MUCWIZARD_MODEMLIST, OnSelectModemlist)
	ON_LBN_SELCHANGE(IDC_MUCWIZARD_ACCTLIST, OnSelectAcctlist)
	ON_BN_CLICKED(IDC_DIALER_FLAG, OnCheckDialerFlag)
#ifdef XP_WIN32
	ON_BN_CLICKED(ID_WIZFINISH,DoFinish)
#endif
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMucEditPage message handlers

BOOL CMucEditPage::OnInitDialog() 
{
	if(!m_bEditView)
	{   
		if(((m_pViewParent->m_pAcctName).Compare("") == 0) ||
			((m_pViewParent->m_pAcctName).Compare("None") == 0))
				m_bCheckState = FALSE;
		else
			m_bCheckState = TRUE;
			
		((CButton*)GetDlgItem(IDC_DIALER_FLAG))->SetCheck(m_bCheckState);
		SetViewPageState(m_bCheckState);

		CString text;
		text.LoadString(IDS_PEMUCVIEW_TEXT1);
		GetDlgItem(IDC_VIEW_TEXT1)->SetWindowText(text);
		text.LoadString(IDS_PEMUCVIEW_TEXT2);
		GetDlgItem(IDC_VIEW_TEXT2)->SetWindowText(text);
		GetDlgItem(IDC_EDIT_TEXT)->ShowWindow(FALSE);
    }
    else 
    {
		GetDlgItem(IDC_DIALER_FLAG)->ShowWindow(FALSE);
		GetDlgItem(IDC_DIALER_TEXT)->ShowWindow(FALSE);
		GetDlgItem(IDC_CTRL)->ShowWindow(FALSE);
		GetDlgItem(IDC_VIEW_TEXT1)->ShowWindow(FALSE);
		GetDlgItem(IDC_VIEW_TEXT2)->ShowWindow(FALSE);
	}
	    
	if(!m_pMucProc.LoadMuc())
		return FALSE;

	m_pMucProc.GetAcctArray(&m_acctList);

	if(!m_bEditView)
	{
		// check the validility
		if(m_pMucProc.IsAcctValid((char*)((const char*)m_pViewParent->m_pAcctName)))
			m_acctSelect = m_pViewParent->m_pAcctName;
	}

	UpdateList();

    RECT        rect;
#ifdef XP_WIN32
	return CNetscapePropertyPage::OnInitDialog();
#else
	if(m_bEditView)
		m_pEditParent->GetWindowRect(&rect);
	else
		m_pViewParent->GetWindowRect(&rect);

	m_height = rect.bottom-rect.top-80;
	m_width = rect.right-rect.left-40;
		
	if(m_bEditView)
		SetWindowPos(&wndTop, rect.left+15, rect.top+30, m_width, m_height, SWP_HIDEWINDOW);
	else
		SetWindowPos(&wndTop, rect.left+15, rect.top+30, m_width, m_height, SWP_SHOWWINDOW);
	
	return TRUE;  
#endif

}
void CMucEditPage::SetMove(int x, int y, int nShowCmd)
{
	RECT    rect;

	if(m_bEditView)
		m_pEditParent->GetWindowRect(&rect);
	else
		m_pViewParent->GetWindowRect(&rect);

	SetWindowPos(&wndTop, rect.left+15, rect.top+30, m_width, m_height, nShowCmd);
}       

void CMucEditPage::UpdateList()
{
	int             num,i,err;
	CString temp;

	m_acctName= (CListBox*)GetDlgItem(IDC_MUCWIZARD_ACCTLIST);
	if(m_acctName == NULL && !(::IsWindow(((CWnd*)m_acctName)->GetSafeHwnd())))
		return;    
	
// update account list
	num = m_acctList.GetSize();
	if(num == 0)
		return;

	for (i=0; i < num; i++) 
	{
		temp = m_acctList.GetAt(i); 
		err = m_acctName->AddString(temp); 
		ASSERT(err != CB_ERR);
	}

	// highlight
	if((!m_bEditView) && (m_acctSelect.GetLength() !=0))
		temp = m_acctSelect;
	else      
//		temp = (char*)(m_acctName->GetItemDataPtr(0));
		temp = m_acctList.GetAt(0); 

	m_acctName->SelectString(-1, temp);
	
	if(m_bEditView)
		m_pEditParent->m_pAcctName = temp;
	else
		m_pViewParent->m_pAcctName = temp;
}

void CMucEditPage::OnSelectAcctlist() 
{
	CString temp; 

	int cur_sel = m_acctName->GetCurSel();

	m_acctName->GetText(cur_sel, temp);
		
	if(m_bEditView)
		m_pEditParent->m_pAcctName = temp;
	else
		m_pViewParent->m_pAcctName = temp;
}

void CMucEditPage::OnCheckDialerFlag() 
{
	m_bCheckState = (m_bCheckState + 1) %2;
	SetViewPageState(m_bCheckState);
}

void CMucEditPage::SetViewPageState(BOOL m_bState) 
{
	((CButton*)GetDlgItem(IDC_DIALER_FLAG))->SetCheck(m_bState);

	GetDlgItem(IDC_MUCEDIT_TEXT1)->EnableWindow(m_bState);
	GetDlgItem(IDC_MUCEDIT_TEXT2)->EnableWindow(m_bState);
	GetDlgItem(IDC_VIEW_TEXT1)->EnableWindow(m_bState);
	GetDlgItem(IDC_VIEW_TEXT2)->EnableWindow(m_bState);
	GetDlgItem(IDC_MUCWIZARD_ACCTLIST)->EnableWindow(m_bState);
}

void CMucEditPage::DoFinish() 
{
	char    acctStr[MAX_PATH];
	char    path[MAX_PATH];
	int     ret;
	BOOL    m_bDialOnDemand = FALSE;
	XP_StatStruct statinfo; 

	if(!m_bEditView)	// advanced option of profile manager
	{
		if(m_bCheckState)
			strcpy(acctStr, (const char*)m_pViewParent->m_pAcctName); 
		else
			strcpy(acctStr, "None");

		m_bDialOnDemand = m_bCheckState;
		strcpy(path, (const char*)m_pViewParent->m_pProfileName);
	}
	else
	{
		// profile dir created?
		ret = _stat((const char*)m_pEditParent->m_pProfilePath, &statinfo);
		if(ret == -1) 
			return;

		// construct config name
		strcpy(path, m_pEditParent->m_pProfilePath);
		strcat(path, "\\config.ini");

		// save the acct config
		if(m_pEditParent->m_pAcctName.GetLength() == 0)
			strcpy(acctStr, "None");
		else
		{
			strcpy(acctStr, (const char*)m_pEditParent->m_pAcctName);
			m_bDialOnDemand = TRUE;
		}
	}

	WritePrivateProfileString("Account", "Account", acctStr, path);

	(*(m_pMucProc.m_lpfnPEPluginFunc))(kSelectDialOnDemand, acctStr, &m_bDialOnDemand);
}

/////////////////////////////////////////////////////////////////////////////
// CMucReadyPage
#ifdef XP_WIN32
CMucReadyPage::CMucReadyPage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
#else
CMucReadyPage::CMucReadyPage(CWnd *pParent)
	: CDialog()
#endif
{
	m_pParent = (CNewProfileWizard*)pParent;
}

#ifdef XP_WIN32
BOOL CMucReadyPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
    return CNetscapePropertyPage::OnSetActive();
}
#else
BOOL CMucReadyPage::Create(UINT nID, CWnd *pWnd)
{       
	return CDialog::Create(nID, pWnd);      
}
#endif

void CMucReadyPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CMucReadyPage::OnInitDialog() 
{
#ifdef XP_WIN32
	return CNetscapePropertyPage::OnInitDialog();
#else
	RECT    rect;
	m_pParent->GetWindowRect(&rect);
	m_height = rect.bottom-rect.top-80;
	m_width = rect.right-rect.left-10;
		
	SetWindowPos(&wndTop, rect.left, rect.top+30, m_width, m_height, SWP_HIDEWINDOW);

	return TRUE;  
#endif
}

void CMucReadyPage::SetMove(int x, int y, int nShowCmd)
{
	SetWindowPos(&wndTop, x, y, m_width, m_height, nShowCmd);
}       

#ifdef XP_WIN32
BEGIN_MESSAGE_MAP(CMucReadyPage, CNetscapePropertyPage)
#else
BEGIN_MESSAGE_MAP(CMucReadyPage, CDialog)
#endif
END_MESSAGE_MAP()
/////////////////////////////////////////////////////////////////////////////
// CASWReadyPage
#ifdef XP_WIN32
CASWReadyPage::CASWReadyPage(CWnd *pParent)
	: CNetscapePropertyPage(IDD)
#else
CASWReadyPage::CASWReadyPage(CWnd *pParent)
	: CDialog()
#endif
{
	m_pParent = (CNewProfileWizard*)pParent;
}


#ifdef XP_WIN32
BOOL CASWReadyPage::OnSetActive()
{
	m_pParent->SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);
    return CNetscapePropertyPage::OnSetActive();
}
#else
BOOL CASWReadyPage::Create(UINT nID, CWnd *pWnd)
{       
	return CDialog::Create(nID, pWnd);      
}
#endif

void CASWReadyPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CASWReadyPage::DoFinish()
{
	char *asw = GetASWURL();
	theApp.m_csPEPage = asw;
	theApp.m_bAlwaysDockTaskBar = TRUE;
	theApp.m_bAccountSetupStartupJava = TRUE;  // Force java startup too
	theApp.m_bAccountSetup = TRUE;       // Force into account setup
	XP_FREEIF(asw);
}

BOOL CASWReadyPage::OnInitDialog() 
{
#ifdef XP_WIN32
	return CNetscapePropertyPage::OnInitDialog();
#else
	RECT    rect;
	m_pParent->GetWindowRect(&rect);
	m_height = rect.bottom-rect.top-80;
	m_width = rect.right-rect.left-10;
		
	SetWindowPos(&wndTop, rect.left, rect.top+30, m_width, m_height, SWP_HIDEWINDOW);

	return TRUE;  
#endif
}

void CASWReadyPage::SetMove(int x, int y, int nShowCmd)
{
	SetWindowPos(&wndTop, x, y, m_width, m_height, nShowCmd);
}       
	
#ifdef XP_WIN32
BEGIN_MESSAGE_MAP(CASWReadyPage, CNetscapePropertyPage)
	ON_BN_CLICKED(ID_WIZFINISH,DoFinish)
	END_MESSAGE_MAP()
#else
BEGIN_MESSAGE_MAP(CASWReadyPage, CDialog)
END_MESSAGE_MAP()
#endif

/////////////////////////////////////////////////////////////////////////////
// CMucViewWizard   
#ifdef XP_WIN32
	       
CMucViewWizard::CMucViewWizard(CWnd *pParent, CString strProfileName,
							   CString strAcctName, CString strModemName)
	: CNetscapePropertySheet("", pParent)
{
	m_pProfileName = strProfileName;
	m_pAcctName = strAcctName;
	m_pMucEditPage = NULL;

	m_pMucEditPage = new CMucEditPage(this,FALSE);
	AddPage(m_pMucEditPage);
	SetWizardMode();
}

CMucViewWizard::~CMucViewWizard()
{
	if (m_pMucEditPage)
		delete m_pMucEditPage;
}

BOOL CMucViewWizard::OnInitDialog()
{
	BOOL ret = CNetscapePropertySheet::OnInitDialog();
	
	CString text;
	text.LoadString(IDS_PEMUCWIZ_TITLE);
	SetTitle(text);

	text.LoadString(IDS_PEMUCWIZ_BUTTON);
	SetFinishText(text);

	GetDlgItem(IDHELP)->ShowWindow(SW_HIDE);
	return ret;  // return TRUE  unless you set the focus to a control
}

void CMucViewWizard::DoFinish()
{
	if (m_pMucEditPage && ::IsWindow(m_pMucEditPage->GetSafeHwnd()))
					m_pMucEditPage->DoFinish();

	PressButton(PSBTN_FINISH);
}

BEGIN_MESSAGE_MAP(CMucViewWizard, CNetscapePropertySheet)
	ON_BN_CLICKED(ID_WIZFINISH,DoFinish)
END_MESSAGE_MAP()

#else
/////////////////////////////////////////////////////////////////////////////
// CMucViewWizard   
CMucViewWizard::CMucViewWizard(CWnd* pParent,CString strProfileName,
							   CString strAcctName, CString strModemName)
	: CDialog(CMucViewWizard::IDD, pParent)
{
	m_pProfileName = strProfileName;
	m_pAcctName = strAcctName;
	m_pModemName = strModemName;

	m_pMucEditPage = new CMucEditPage(this,FALSE);
}

void CMucViewWizard::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CMucViewWizard::OnInitDialog()
{
	CDialog::OnInitDialog();
	
	m_pMucEditPage->Create(IDD_MUCWIZARD_EDIT, this);
			
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMucViewWizard::OnMove(int x, int y)
{
	CDialog::OnMove(x, y);

	if(m_pMucEditPage)
		m_pMucEditPage->SetMove(x,y,SW_SHOW);

}
void CMucViewWizard::OnOK()
{ 
	if (m_pMucEditPage && ::IsWindow(m_pMucEditPage->GetSafeHwnd()))
		m_pMucEditPage->DoFinish();

	EndDialog(TRUE);
}

void CMucViewWizard::OnCancel()
{   
	EndDialog(TRUE);
}

BEGIN_MESSAGE_MAP(CMucViewWizard, CDialog)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
	ON_BN_CLICKED(IDOK,OnOK)
	ON_WM_MOVE()
END_MESSAGE_MAP()


#endif XP_WIN32

