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

// dspppage.cpp : implementation file
//

#include "stdafx.h"
#include "dspppage.h"
#include "mailpriv.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define DEF_DAYS 30
#define DEF_KEEP_NEW 500
/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePropertyPage property page
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

CDiskSpacePropertyPage::CDiskSpacePropertyPage(CNewsFolderPropertySheet *pParent,BOOL bDefaultPref,int nSelection, int nDays, int nKeepNew, BOOL bKeepUnread) 
						: CNetscapePropertyPage(CDiskSpacePropertyPage::IDD)
{
	//{{AFX_DATA_INIT(CDiskSpacePropertyPage)
	m_bCheckDefault = bDefaultPref;
	m_nKeepSelection= nSelection;
	m_nDays = DEF_DAYS;//nDays;
	m_nKeepNew = DEF_KEEP_NEW;//nKeepNew;
	m_bCheckUnread = bKeepUnread;
	//}}AFX_DATA_INIT
	m_pParent = (CNewsFolderPropertySheet*)pParent;
	m_pMoreDlg =  new CMoreChoicesDlg((CWnd*)this);
	m_folderInfo = NULL;
}

CDiskSpacePropertyPage::~CDiskSpacePropertyPage()
{
	if (m_pMoreDlg)
		delete m_pMoreDlg;
}

void CDiskSpacePropertyPage::SetFolderInfo( MSG_FolderInfo *folderInfo )
{
	m_folderInfo = folderInfo;
}

BOOL CDiskSpacePropertyPage::OnSetActive()
{
	return CPropertyPage::OnSetActive();
}

void CDiskSpacePropertyPage::DisableOthers(BOOL bChecked)
{
	//this is a toggle to the other windows of what
	//ever the current state of IDC_CHECK2_DEFAULT is.  
	EnableDisableItem(!bChecked,IDC_BNT_MORE);
    EnableDisableItem(!bChecked,IDC_EDIT_DAYS);
	EnableDisableItem(!bChecked,IDC_RADIO_KEEP1);
	EnableDisableItem(!bChecked,IDC_RADIO_KEEP2);
	EnableDisableItem(!bChecked,IDC_RADIO_KEEP3);
	EnableDisableItem(!bChecked,IDC_EDIT_NEWEST);
	EnableDisableItem(!bChecked,IDC_CHECK_KEEP4);
}


BOOL CDiskSpacePropertyPage::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();
	MSG_PurgeByPreferences	purgeBy;
	int32	nDays;
	int32	nKeepNew;

	//get the default start state
	MSG_GetHeaderPurgingInfo(m_folderInfo, &m_bCheckDefault, &purgeBy,	&m_bCheckUnread, &nDays,	&nKeepNew);

	m_nDays = CASTINT(nDays);
	m_nKeepNew = CASTINT(nKeepNew);

	switch ( purgeBy )
	{
	case MSG_PurgeByAge:
		m_nKeepSelection = 0;
		break;
	case MSG_PurgeNone:
		m_nKeepSelection = 1;
		break;
	case MSG_PurgeByNumHeaders:
		m_nKeepSelection = 2;
		break;
	}
	if (m_hWnd != NULL || IsWindow(m_hWnd))
		UpdateData(FALSE);

	CheckDlgButton(IDC_CHECK_KEEP4,m_bCheckUnread);
	return ret;
}


void CDiskSpacePropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDiskSpacePropertyPage)
    DDX_Radio(pDX,IDC_RADIO_KEEP1,m_nKeepSelection);
	DDX_Text(pDX, IDC_EDIT_DAYS, m_nDays);
	DDV_MinMaxUInt(pDX, m_nDays, 0, 2000);
	DDX_Text(pDX, IDC_EDIT_NEWEST, m_nKeepNew);
	DDV_MinMaxUInt(pDX, m_nKeepNew, 0, 2000);
	DDX_Check(pDX,IDC_CHECK_KEEP4,m_bCheckUnread);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDiskSpacePropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CDiskSpacePropertyPage)
	ON_BN_CLICKED(IDC_BNT_MORE, OnBtnMore)
	ON_BN_CLICKED(IDC_DOWNLOAD_NOW, OnDownloadNow)
	ON_EN_CHANGE(IDC_EDIT_DAYS, OnChangeEditDays)
	ON_BN_CLICKED(IDC_RADIO_KEEP1, OnRdKeepDays)
	ON_BN_CLICKED(IDC_RADIO_KEEP3, OnRdKeepNew)
	ON_BN_CLICKED(IDC_RADIO_KEEP2, OnRdKeepall)
	ON_EN_CHANGE(IDC_EDIT_NEWEST, OnChangeEditNewest)
    ON_BN_CLICKED(IDC_CHECK_KEEP4,OnCheckUnread)

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDiskSpacePropertyPage message handlers
void CDiskSpacePropertyPage::EnableDisableItem(BOOL bState, UINT nIDC)
{   //disable or enable a dialog item window
	CWnd *pWnd = GetDlgItem(nIDC);
	if (pWnd)
	{
		pWnd->EnableWindow(bState);
	}
}

void CDiskSpacePropertyPage::OnCheckUnread()
{
	SetModified();
}


void CDiskSpacePropertyPage::OnBtnMore() 
{
	if(m_pMoreDlg)
	{
		MSG_PurgeByPreferences	purgeBy = MSG_PurgeNone;
		int32	nDays = 30;
		XP_Bool	useDefaults;

		MSG_GetArticlePurgingInfo(m_folderInfo, &useDefaults, &purgeBy, &nDays);
		m_pMoreDlg->m_bCheck = (purgeBy == MSG_PurgeByAge) ? TRUE : FALSE;
		m_pMoreDlg->m_nDays = CASTINT(nDays);
		if (m_pMoreDlg->DoModal() == IDOK)
		{
			purgeBy = (m_pMoreDlg->m_bCheck) ? MSG_PurgeByAge : MSG_PurgeNone;
			nDays = m_pMoreDlg->m_nDays;
			MSG_SetArticlePurgingInfo(m_folderInfo, FALSE, purgeBy, nDays);
		}
	}
}


void CDiskSpacePropertyPage::OnDownloadNow()
{
	m_pParent->OnDownLoadButton();
}


void CDiskSpacePropertyPage::OnChangeEditDays() 
{
	UpdateData();
	m_nKeepSelection=0;
	UpdateData(FALSE);
	SetModified();
}

void CDiskSpacePropertyPage::OnRdKeepDays() 
{
	CWnd *pWnd = GetDlgItem(IDC_EDIT_DAYS);
	if (pWnd)
	    GotoDlgCtrl(pWnd);
	SetModified();
}

void CDiskSpacePropertyPage::OnRdKeepNew() 
{
	CWnd *pWnd = GetDlgItem(IDC_EDIT_NEWEST);
	if (pWnd)
	    GotoDlgCtrl(pWnd);
	SetModified();
}

void CDiskSpacePropertyPage::OnRdKeepall() 
{
	SetModified();
}

void CDiskSpacePropertyPage::OnChangeEditNewest() 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	m_nKeepSelection=2;
	UpdateData(FALSE);
	SetModified();
}


void CDiskSpacePropertyPage::OnOK() 
{
	if (m_hWnd == NULL || !IsWindow(m_hWnd))
	{
		CNetscapePropertyPage::OnOK();
		return;
	}

	MSG_PurgeByPreferences	purgeBy = MSG_PurgeNone;


	UpdateData();

	switch ( m_nKeepSelection )
	{
	case 0:
		purgeBy = MSG_PurgeByAge;
		break;
	case 1:
		purgeBy = MSG_PurgeNone;
		break;
	case 2:
		purgeBy = MSG_PurgeByNumHeaders;
		break;
	default:
		XP_ASSERT(FALSE);
		break;
	}

	//we do not allow them to change toggle from default to manual settings.
	//defaults only good for newly subscribed groups. Once manual settings are
	//applied, they stay applied for now.
	MSG_SetHeaderPurgingInfo(m_folderInfo, FALSE, purgeBy, m_bCheckUnread, (int32) m_nDays,	(int32) m_nKeepNew);
	CNetscapePropertyPage::OnOK();
}



/////CMoreChoicesDlg: called from CDiskSpacePropertyPage

CMoreChoicesDlg::CMoreChoicesDlg(CWnd* pParent)
	: CDialog(CMoreChoicesDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMoreChoicesDlg)
	m_bCheck = TRUE;
	m_nDays = DEF_DAYS;
	m_pParent = (CDiskSpacePropertyPage*)pParent;
	//}}AFX_DATA_INIT
}


void CMoreChoicesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMoreChoicesDlg)
	DDX_Check(pDX, IDC_CHECK1, m_bCheck);
	DDX_Text(pDX, IDC_EDIT1, m_nDays);
	DDV_MinMaxUInt(pDX, m_nDays, 0, 2000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMoreChoicesDlg, CDialog)
	//{{AFX_MSG_MAP(CMoreChoicesDlg)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck)
	ON_EN_CHANGE(IDC_EDIT1, OnChangeEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMoreChoicesDlg message handlers

void CMoreChoicesDlg::OnCheck() 
{
	// TODO: Add your control notification handler code here
	if (m_pParent) 
		m_pParent->SetModified();
	CWnd *pWnd = GetDlgItem(IDC_EDIT1);
	if (pWnd)
	    GotoDlgCtrl(pWnd);	
}

void CMoreChoicesDlg::OnChangeEdit() 
{
	// TODO: Add your control notification handler code here
  	UpdateData();
	if (m_pParent) 
		m_pParent->SetModified();
	UpdateData(FALSE);
	CheckDlgButton(IDC_CHECK1,TRUE);
}

void CMoreChoicesDlg::OnCancel() 
{
	CDialog::OnCancel();
	if (m_pParent) 
		m_pParent->SetModified(FALSE);
}

void CMoreChoicesDlg::OnOK() 
{
	CDialog::OnOK();
}






/////////////////////////////////////////////////////////////////////////////
// CDownLoadPPNews 
// The download options property page used for Mail and News properties.


CDownLoadPPNews::CDownLoadPPNews(CNewsFolderPropertySheet *pParent) : CNetscapePropertyPage(CDownLoadPPNews::IDD)
{
	//{{AFX_DATA_INIT(CNetscapePropertyPage)
	m_bCheckArticles = FALSE;
	m_bCheckDefault = FALSE;
	m_bCheckDownLoad = FALSE;
	m_bCheckByDate = FALSE;
	m_nDaysAgo = 0;
	m_nRadioValue = -1;
	//}}AFX_DATA_INIT
	m_pParent = (CNewsFolderPropertySheet*)pParent;
}

CDownLoadPPNews::~CDownLoadPPNews()
{

}
void CDownLoadPPNews::SetFolderInfo( MSG_FolderInfo *folderInfo )
{
	m_folderInfo = folderInfo;
}

void CDownLoadPPNews::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDownLoadPPNews)
	DDX_Check(pDX, IDC_CHECK_BY_ARTICLES, m_bCheckArticles);
	DDX_Check(pDX, IDC_CHECK_DEFAULT, m_bCheckDefault);
	DDX_Check(pDX, IDC_CHECK_DOWNLOAD, m_bCheckDownLoad);
	DDX_Check(pDX, IDC_CHECK4_BY_DATE, m_bCheckByDate);
	DDX_Text(pDX, IDC_EDIT_DAYS_AGO, m_nDaysAgo);
	DDV_MinMaxUInt(pDX, m_nDaysAgo, 0, 1000);
	DDX_Radio(pDX, IDC_RADIO_FROM, m_nRadioValue);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDownLoadPPNews, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_DOWNLOAD_NOW, OnDownloadNow)
	ON_BN_CLICKED(IDC_CHECK_DEFAULT, OnCheckDefault)
	ON_CBN_SETFOCUS(IDC_COMBO2, OnSetfocusCombo2)
	ON_CBN_SELCHANGE(IDC_COMBO2, OnSelchangeCombo2)
	ON_EN_CHANGE(IDC_EDIT_DAYS_AGO, OnChangeEditDaysAgo)
	ON_BN_CLICKED(IDC_RADIO_SINCE, OnRadioSince)
	ON_BN_CLICKED(IDC_RADIO_FROM, OnRadioFrom)
	ON_BN_CLICKED(IDC_CHECK_BY_ARTICLES, OnCheckByArticles)
	ON_BN_CLICKED(IDC_CHECK4_BY_DATE,OnCheckByDate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDownLoadPPNews message handlers

void CDownLoadPPNews::OnDownloadNow()
{
	m_pParent->OnDownLoadButton();
}

void CDownLoadPPNews::DisableOthers(BOOL bChecked)
{
	//this is a toggle to the other windows of what
	//ever the current state of IDC_CHECK_DEFAULT is.  
	EnableDisableItem(!bChecked,IDC_COMBO2);
	EnableDisableItem(!bChecked,IDC_EDIT_DAYS_AGO);
	EnableDisableItem(!bChecked,IDC_RADIO_SINCE);
	EnableDisableItem(!bChecked,IDC_RADIO_FROM);
	EnableDisableItem(!bChecked,IDC_CHECK_BY_ARTICLES);
	EnableDisableItem(!bChecked,IDC_CHECK4_BY_DATE);
}


void CDownLoadPPNews::EnableDisableItem(BOOL bState, UINT nIDC)
{   //disable or enable a dialog item window
	CWnd *pWnd = GetDlgItem(nIDC);
	if (pWnd)
	{
		pWnd->EnableWindow(bState);
	}
}

void CDownLoadPPNews::OnCheckDefault() 
{
	DisableOthers(IsDlgButtonChecked(IDC_CHECK_DEFAULT));
	UpdateData();
}

void CDownLoadPPNews::OnCheckByDate() 
{
	//we want to default to the from radio on check by date
	if (IsDlgButtonChecked(IDC_CHECK4_BY_DATE))
	{
		if (IsDlgButtonChecked(IDC_RADIO_SINCE))
			CheckDlgButton(IDC_RADIO_SINCE,FALSE);

		CWnd *pWnd = GetDlgItem(IDC_COMBO2);
		CheckDlgButton(IDC_RADIO_FROM,TRUE);
		if (pWnd)
			GotoDlgCtrl(pWnd);
		SetModified();
	}
	UpdateData();
}



void CDownLoadPPNews::OnSetfocusCombo2() 
{
	// TODO: Add your control notification handler code here

	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_COMBO2);
	if (pCombo)
		pCombo->SetCurSel(0);
	UpdateData();
}

void CDownLoadPPNews::OnSelchangeCombo2() 
{
	// TODO: Add your control notification handler code here
	if (IsDlgButtonChecked(IDC_RADIO_SINCE))
	{
		CheckDlgButton(IDC_RADIO_SINCE,FALSE);
		CheckDlgButton(IDC_RADIO_FROM,TRUE);
	}
    
	if (!IsDlgButtonChecked(IDC_CHECK4_BY_DATE))
		CheckDlgButton(IDC_CHECK4_BY_DATE,TRUE);

	UpdateData();
	SetModified();
}

void CDownLoadPPNews::OnChangeEditDaysAgo() 
{

	BOOL ret = UpdateData();
	m_nRadioValue = 1;
	SetModified();	
	UpdateData(FALSE);
	if(!IsDlgButtonChecked(IDC_CHECK4_BY_DATE))
		CheckDlgButton(IDC_CHECK4_BY_DATE,TRUE);
}

void CDownLoadPPNews::OnRadioSince() 
{
	// TODO: Add your control notification handler code here
	CWnd *pWnd = GetDlgItem(IDC_EDIT_DAYS_AGO);
	if (pWnd)
	    GotoDlgCtrl(pWnd);

	UpdateData();
	SetModified();
}

void CDownLoadPPNews::OnRadioFrom() 
{
	CWnd *pWnd = GetDlgItem(IDC_COMBO2);
	if (pWnd)
		GotoDlgCtrl(pWnd);
	SetModified();	
}

void CDownLoadPPNews::OnCheckByArticles() 
{
	// TODO: Add your control notification handler code here
	UpdateData();
	SetModified();
}

void CDownLoadPPNews::OnOK() 
{
	if (m_hWnd == NULL || !IsWindow(m_hWnd))
	{
		CNetscapePropertyPage::OnOK();
		return;
	}
	UpdateData(TRUE);


	if (m_nRadioValue  == 0)
	{
		CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_COMBO2);
		if (pCombo)
		{
			switch (pCombo->GetCurSel())
			{
			case 0:
				m_nDaysAgo = 1;
				break;
			case 1:
				m_nDaysAgo = 7;
				break;
			case 2:
				m_nDaysAgo = 14;
				break;
			case 3:
				m_nDaysAgo = 31;
				break;
			case 4:
				m_nDaysAgo = 183;
				break;
			case 5:
				m_nDaysAgo = 365;
				
			}
		}
	}
	if (m_bCheckDownLoad)
		MSG_SetFolderPrefFlags(m_folderInfo, MSG_GetFolderPrefFlags(m_folderInfo) | MSG_FOLDER_PREF_OFFLINE);
	else
		MSG_SetFolderPrefFlags(m_folderInfo, MSG_GetFolderPrefFlags(m_folderInfo) & ~MSG_FOLDER_PREF_OFFLINE);

	MSG_SetOfflineRetrievalInfo(m_folderInfo, m_bCheckDefault, m_bCheckArticles, m_bCheckArticles, m_bCheckByDate, m_nDaysAgo);

	CNetscapePropertyPage::OnOK();
}

BOOL CDownLoadPPNews::OnInitDialog() 
{
	BOOL bRet = CNetscapePropertyPage::OnInitDialog();
	XP_Bool bUnreadOnly = FALSE;
	int	iComboSel = -1;
	int32	i32DaysAgo;
	MSG_GetOfflineRetrievalInfo(m_folderInfo, &m_bCheckDefault, &m_bCheckArticles, &bUnreadOnly, &m_bCheckByDate, &i32DaysAgo);


	CheckDlgButton(IDC_CHECK_BY_ARTICLES,m_bCheckArticles);
	CheckDlgButton(IDC_CHECK4_BY_DATE,m_bCheckByDate);
	
	switch (i32DaysAgo)
	{
	case 1:
		iComboSel = 0;
		break;
	case 7:
		iComboSel = 1;
		break;
	case 14:
		iComboSel = 2;
		break;
	case 31:
		iComboSel = 3;
		break;
	case 183:
		iComboSel = 4;
		break;
	case 365:
		iComboSel = 5;
	}
	if (m_bCheckByDate) {
		if (iComboSel != -1) {
			CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_COMBO2);
			if (pCombo) {
				pCombo->SetCurSel(iComboSel);
			}
			m_nRadioValue = 0;
			CheckDlgButton(IDC_RADIO_FROM,TRUE);
		}
		else
		{
			m_nRadioValue = 1;
			m_nDaysAgo=i32DaysAgo;
			CheckDlgButton(IDC_RADIO_SINCE,TRUE);
		}
	}
	m_bCheckDownLoad = MSG_GetFolderPrefFlags(m_folderInfo) & MSG_FOLDER_PREF_OFFLINE;
	CheckDlgButton(IDC_CHECK_DOWNLOAD,m_bCheckDownLoad);
	CheckDlgButton(IDC_CHECK_DEFAULT,m_bCheckDefault);
	DisableOthers(m_bCheckDefault);
	UpdateData(FALSE);
	return bRet;
}

BOOL CDownLoadPPNews::OnSetActive() 
{
	return CNetscapePropertyPage::OnSetActive();	
}




/////////////////////////////////////////////////////////////////////////////
// CDownLoadPPNews 
// The download options property page used for Mail and News properties.


CDownLoadPPMail::CDownLoadPPMail() : CNetscapePropertyPage(CDownLoadPPMail::IDD)
{
	//{{AFX_DATA_INIT(CNetscapePropertyPage)
	m_bCheckDownLoadMail = FALSE;
	//}}AFX_DATA_INIT
}

CDownLoadPPMail::~CDownLoadPPMail()
{
}

void CDownLoadPPMail::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDownLoadPPMail)
	DDX_Check(pDX, IDC_CHECK1, m_bCheckDownLoadMail);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDownLoadPPMail, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_CHECK_DEFAULT, OnCheckDownLoad)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDownLoadPPNews message handlers

void CDownLoadPPMail::OnCheckDownLoad() 
{
}

void CDownLoadPPMail::SetFolderInfo(MSG_FolderInfo *pfolderInfo)
{
	m_pfolderInfo = pfolderInfo;
}

void CDownLoadPPMail::OnOK() 
{
	if (m_hWnd == NULL || !IsWindow(m_hWnd))
	{
		CNetscapePropertyPage::OnOK();
		return;
	}
	CNetscapePropertyPage::OnOK();

	MSG_SetFolderPrefFlags(m_pfolderInfo, (m_bCheckDownLoadMail == 1 ? MSG_FOLDER_PREF_OFFLINE : !MSG_FOLDER_PREF_OFFLINE) );

	// TODO: Add your specialized code here and/or call the base class
}

BOOL CDownLoadPPMail::OnInitDialog() 
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();
	// TODO: Add your specialized code here and/or call the base class
	m_bCheckDownLoadMail = MSG_FOLDER_PREF_OFFLINE & MSG_GetFolderPrefFlags(m_pfolderInfo);

	UpdateData(FALSE);
	return ret;
}
