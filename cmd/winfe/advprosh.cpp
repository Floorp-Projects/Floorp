/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
#include "windowsx.h"

#include "wfemsg.h"
#include "prefapi.h"

#include "nethelp.h"
#include "advprosh.h"

/*-----------------------------------------------------------------*/
/*                                                                 */
/* Search options page                                             */
/*                                                                 */
/*-----------------------------------------------------------------*/

CSearchOptionsPage::CSearchOptionsPage(CWnd *pParent, UINT nID)
	: CNetscapePropertyPage(nID)
{
	m_pParent = pParent;
  m_bHasBeenViewed = FALSE;
}

CSearchOptionsPage::~CSearchOptionsPage()
{
}

void CSearchOptionsPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAdvSearchOptionsDlg)
	DDX_Check(pDX, IDC_CHECK_SUBFOLDERS, m_bIncludeSubfolders);
	DDX_Check(pDX, IDC_CHECK_SEARCH_LOCALLY, m_iSearchArea);
	//}}AFX_DATA_MAP
}

BOOL CSearchOptionsPage::OnInitDialog()
{
	XP_Bool bSubFolders   = FALSE;
	XP_Bool bSearchServer = FALSE;

  PREF_GetBoolPref("mailnews.searchSubFolders",&bSubFolders);
	m_bIncludeSubfolders = bSubFolders;

  m_bOffline = NET_IsOffline();

  if(!m_bOffline)
    PREF_GetBoolPref("mailnews.searchServer",&bSearchServer);
  else
  {
    CWnd * pCheck = GetDlgItem(IDC_CHECK_SEARCH_LOCALLY);
    if(pCheck != NULL)
      pCheck->EnableWindow(FALSE);
  }

  m_iSearchArea = bSearchServer ? 0 : 1; 

  UpdateData(FALSE);
  
  m_bHasBeenViewed = TRUE;

  BOOL ret = CNetscapePropertyPage::OnInitDialog();
	return ret;
}

void CSearchOptionsPage::OnOK()
{
  if(!m_bHasBeenViewed)
    return;
	UpdateData();
	PREF_SetBoolPref("mailnews.searchSubFolders",(XP_Bool)m_bIncludeSubfolders);

  if(!m_bOffline)
  {
    XP_Bool bSearchServer = m_iSearchArea ? 0 : 1;
	  PREF_SetBoolPref("mailnews.searchServer", bSearchServer);
  }
}

void CSearchOptionsPage::OnCancel()
{
  CNetscapePropertyPage::OnCancel();
}

BOOL CSearchOptionsPage::OnSetActive()
{
  if(!CNetscapePropertyPage::OnSetActive())
    return FALSE;

  return TRUE ;
}

BOOL CSearchOptionsPage::OnKillActive()
{
	return CNetscapePropertyPage::OnKillActive();
}


BEGIN_MESSAGE_MAP(CSearchOptionsPage, CNetscapePropertyPage)
END_MESSAGE_MAP()

/*-----------------------------------------------------------------*/
/*                                                                 */
/* Edit headers page                                               */
/*                                                                 */
/*-----------------------------------------------------------------*/

CCustomHeadersPage::CCustomHeadersPage(CWnd *pParent, UINT nID)
	: CNetscapePropertyPage(nID)
{
	m_pParent = pParent;
  m_bHasBeenViewed = FALSE;
}

CCustomHeadersPage::~CCustomHeadersPage()
{
}

void CCustomHeadersPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CCustomHeadersPage::OnInitDialog()
{
  MSG_FolderInfo *pInbox = NULL;
	MSG_GetFoldersWithFlag (WFE_MSGGetMaster(), MSG_FOLDER_FLAG_INBOX, &pInbox, 1);
	uint16 numItems;
	MSG_GetNumAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, &numItems); 
	MSG_SearchMenuItem * pHeaderItems = new MSG_SearchMenuItem [numItems];
	
	if (!pHeaderItems) 
		return FALSE;

  MSG_GetAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, pHeaderItems, &numItems);

  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

  for (int i=0; i < numItems; i++)
	{
		if ( (pHeaderItems[i].attrib == attribOtherHeader) && pHeaderItems[i].isEnabled )
			pList->AddString(pHeaderItems[i].name);
	}

	delete pHeaderItems;

  updateUI();

  m_bHasBeenViewed = TRUE;

  return CNetscapePropertyPage::OnInitDialog();
}

void CCustomHeadersPage::OnAddHeader() 
{
  CEdit * pEdit = (CEdit *)GetDlgItem(IDC_EDIT_NEW_HEADER);
  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

  char szString[256] = {'\0'};
  pEdit->GetWindowText(szString, sizeof(szString));
  if(strlen(szString) == 0)
    return;
  pList->AddString(szString);
  pEdit->SetWindowText("");
  pEdit->SetFocus();
}

void CCustomHeadersPage::OnRemoveHeader() 
{
  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

	int iSel = pList->GetCurSel();
  if(iSel < 0)
    return;
  
  pList->DeleteString(iSel);

  int iNewCount = pList->GetCount();

  if(iNewCount > 0)
    pList->SetCurSel((iSel >= iNewCount) ? iNewCount - 1 : iSel);

  OnSelChangeHeaderList();

  if(iNewCount == 0)
  {
    CEdit * pEdit = (CEdit *)GetDlgItem(IDC_EDIT_NEW_HEADER);
    pEdit->SetFocus();
  }
}

void CCustomHeadersPage::OnReplaceHeader() 
{
  CEdit * pEdit = (CEdit *)GetDlgItem(IDC_EDIT_NEW_HEADER);
  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

  char szString[256] = {'\0'};
  pEdit->GetWindowText(szString, sizeof(szString));
  if(strlen(szString) == 0)
    return;

  int iSel = pList->GetCurSel();
  if(iSel != LB_ERR)
  {
    pList->DeleteString(iSel);
    pList->InsertString(iSel, szString);
  }
  else
    pList->AddString(szString);

  updateUI();
  pEdit->SetFocus();
}

void CCustomHeadersPage::OnChangeEditHeader() 
{
  updateUI();
  char szString[256];
  GetDlgItemText(IDC_EDIT_NEW_HEADER, szString, sizeof(szString));
  if(strlen(szString) > 0)
    SetDefID(IDC_ADD_HEADER);
}

void CCustomHeadersPage::OnSelChangeHeaderList()
{
  CEdit * pEdit = (CEdit *)GetDlgItem(IDC_EDIT_NEW_HEADER);
  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

  char szString[256] = {'\0'};

  int iSel = pList->GetCurSel();

  if(iSel >= 0)
  {
    pList->GetText(iSel, szString);
    pEdit->SetWindowText(szString);
  }
  else
    pEdit->SetWindowText("");

  updateUI();
}

BOOL CCustomHeadersPage::OnSetActive()
{
  if(!CNetscapePropertyPage::OnSetActive())
    return FALSE;

  return TRUE ;
}

BOOL CCustomHeadersPage::OnKillActive()
{
	return CNetscapePropertyPage::OnKillActive();
}

void CCustomHeadersPage::OnOK() 
{
  if(!m_bHasBeenViewed)
    return;

  CEdit * pEdit = (CEdit *)GetDlgItem(IDC_EDIT_NEW_HEADER);
  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

  int iCount = pList->GetCount();

  if(iCount != 0)
	{
		CString strTemp;
		pList->GetText(0, strTemp);
		CString strHeaderPrefList = strTemp; 

		for (int i = 1; i < iCount; i++)
		{
			pList->GetText(i,strTemp);
			strHeaderPrefList += ": " + strTemp; 
		}
		PREF_SetCharPref("mailnews.customHeaders", strHeaderPrefList);
	}
	else
	{
		PREF_SetCharPref("mailnews.customHeaders","");
	}
	CPropertyPage::OnOK();
}

void CCustomHeadersPage::OnCancel() 
{
	CNetscapePropertyPage::OnCancel();
}

void CCustomHeadersPage::enableDlgItem(int iId, BOOL bEnable)
{
  CWnd * pItem = GetDlgItem(iId);
  if(pItem == NULL)
    return;
  pItem->EnableWindow(bEnable);
}

void CCustomHeadersPage::updateUI() 
{
  CListBox * pList = (CListBox *)GetDlgItem(IDC_HEADER_LIST);

  char szString[256] = {'\0'};
  int iSel = pList->GetCurSel();

  GetDlgItemText(IDC_EDIT_NEW_HEADER, szString, sizeof(szString));
  enableDlgItem(IDC_ADD_HEADER, (strlen(szString) > 0));

  enableDlgItem(IDC_REMOVE_HEADER, (iSel >= 0));

  char szToReplace[256] = {'\0'};
  pList->GetText(iSel, szToReplace);
  enableDlgItem(IDC_REPLACE_HEADER, ((strlen(szString) > 0) && (iSel >= 0) && (0 != strcmp(szString, szToReplace))));
}

BEGIN_MESSAGE_MAP(CCustomHeadersPage, CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_ADD_HEADER, OnAddHeader)
	ON_BN_CLICKED(IDC_REMOVE_HEADER, OnRemoveHeader)
	ON_BN_CLICKED(IDC_REPLACE_HEADER, OnReplaceHeader)
	ON_EN_CHANGE(IDC_EDIT_NEW_HEADER, OnChangeEditHeader)
	ON_LBN_SELCHANGE(IDC_HEADER_LIST, OnSelChangeHeaderList)
END_MESSAGE_MAP()

/*-----------------------------------------------------------------*/
/*                                                                 */
/* Advanced search property sheet                                  */
/*                                                                 */
/*-----------------------------------------------------------------*/

CAdvancedOptionsPropertySheet::CAdvancedOptionsPropertySheet(CWnd *pParent, 
                                                             const char* pName, 
                                                             DWORD dwPagesBitFlag)
	: CNetscapePropertySheet(pName, pParent, 0, NULL),
	m_pParent(pParent),
  m_iReturnCode(IDCANCEL),
  m_dwPagesBitFlag(dwPagesBitFlag),
  m_OptionsPage(NULL),
  m_HeadersPage(NULL)
{
  assert(m_dwPagesBitFlag != AOP_NOPAGES);

  if(m_dwPagesBitFlag & AOP_SEARCH_OPTIONS)
  {
    m_OptionsPage = new CSearchOptionsPage(pParent, IDD_ADVANCED_SEARCH);
    AddPage(m_OptionsPage);
  }

  if(m_dwPagesBitFlag & AOP_CUSTOM_HEADERS)
  {
    m_HeadersPage = new CCustomHeadersPage(pParent, IDD_HEADER_LIST);
    AddPage(m_HeadersPage);
  }
}

CAdvancedOptionsPropertySheet::~CAdvancedOptionsPropertySheet()
{
  if(m_OptionsPage!= NULL)
    delete m_OptionsPage;
  if(m_HeadersPage!= NULL)
    delete m_HeadersPage;
}

CNetscapePropertyPage * CAdvancedOptionsPropertySheet::GetCurrentPage()
{
  return (CNetscapePropertyPage *)GetActivePage();
}

BOOL CAdvancedOptionsPropertySheet::OnCommand(WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
  int id = LOWORD(wParam);
#else
  int id = (int)wParam;
#endif

  if((id == IDOK) || (id == IDCANCEL))
  {
    m_iReturnCode = id;
    OnClose();
    return TRUE;
  }

  return CNetscapePropertySheet::OnCommand(wParam, lParam);
}

#define BUTTON_CONTAINER    33
#define BUTTON_WIDTH        75
#define BUTTON_HEIGHT       23
#define BUTTON_TO_RIGHT     9
#define BUTTON_TO_BOTTOM    10
#define BUTTON_TO_BUTTON    6
#define X_ADJUSTMENT        9
#define Y_ADJUSTMENT        3

#define SetWinFont(hwnd, hfont, fRedraw) FORWARD_WM_SETFONT((hwnd), (hfont), (fRedraw), ::SendMessage)
#define GetWinFont(hwnd) FORWARD_WM_GETFONT((hwnd), ::SendMessage)
  
#ifdef _WIN32
 
BOOL CAdvancedOptionsPropertySheet::OnInitDialog()
{
	BOOL ret = CNetscapePropertySheet::OnInitDialog();

#else

int CAdvancedOptionsPropertySheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int ret = CNetscapePropertySheet::OnCreate(lpCreateStruct);

#endif

  if(m_pParent && ::IsWindow(m_pParent->m_hWnd))
    m_pParent->EnableWindow(FALSE);

  EnableWindow(TRUE);

  RECT rc;
  GetWindowRect(&rc);
  int iWidth = rc.right - rc.left;
  int iHeight = rc.bottom - rc.top + BUTTON_CONTAINER;
  SetWindowPos(NULL,0,0, iWidth, iHeight, SWP_NOZORDER | SWP_NOMOVE);

  DWORD dwStyle = WS_VISIBLE | WS_CHILD;
  HINSTANCE hInst = AfxGetInstanceHandle();

  GetClientRect(&rc);
  HFONT hFont = GetWinFont(m_hWnd);

  int y = rc.bottom - rc.top - BUTTON_TO_BOTTOM - BUTTON_HEIGHT + Y_ADJUSTMENT;
  int w = BUTTON_WIDTH;
  int h = BUTTON_HEIGHT;
  int x;
  HWND hWnd;

  x = rc.right - rc.left - BUTTON_TO_RIGHT - (BUTTON_TO_BUTTON + BUTTON_WIDTH) + X_ADJUSTMENT;
  hWnd = CreateWindow("button", "Help", dwStyle, x,y, w, h, m_hWnd, (HMENU)IDHELP, hInst, NULL);
  SetWinFont(hWnd, hFont, TRUE);

  x -= BUTTON_TO_BUTTON + BUTTON_WIDTH;
  hWnd = CreateWindow("button", "Cancel", dwStyle, x,y, w, h, m_hWnd, (HMENU)IDCANCEL, hInst, NULL);
  SetWinFont(hWnd, hFont, TRUE);

  x -= BUTTON_TO_BUTTON + BUTTON_WIDTH;
  hWnd = CreateWindow("button", "OK", dwStyle, x,y, w, h, m_hWnd, (HMENU)IDOK, hInst, NULL);
  SetWinFont(hWnd, hFont, TRUE);

  return ret;
}

void CAdvancedOptionsPropertySheet::OnDestroy()
{
  CNetscapePropertySheet::OnDestroy();
}

void CAdvancedOptionsPropertySheet::OnClose()
{
  if(m_dwPagesBitFlag & AOP_SEARCH_OPTIONS)
  {
    if(m_iReturnCode == IDOK)
      m_OptionsPage->OnOK();
    m_pParent->PostMessage(WM_ADVANCED_OPTIONS_DONE, 0, m_iReturnCode);
  }

  if(m_dwPagesBitFlag & AOP_CUSTOM_HEADERS)
  {
    if(m_iReturnCode == IDOK)
      m_HeadersPage->OnOK();
    m_pParent->PostMessage(WM_EDIT_CUSTOM_DONE, 0, m_iReturnCode);
  }

  if(m_pParent && ::IsWindow(m_pParent->m_hWnd))
    m_pParent->EnableWindow(TRUE);

  DestroyWindow();
}

void CAdvancedOptionsPropertySheet::OnNcDestroy()
{
  delete this;
}

void CAdvancedOptionsPropertySheet::OnHelp()
{
  if(GetActivePage() == m_OptionsPage)
    NetHelp(HELP_SEARCH_MAILNEWS_OPTIONS);
  if(GetActivePage() == m_HeadersPage)
    NetHelp(HELP_SEARCH_MAILNEWS_HEADERS);
}

BOOL CAdvancedOptionsPropertySheet::PreTranslateMessage(MSG * pMsg)
{
  if(pMsg->message == WM_KEYDOWN)
  {
    switch ((int)pMsg->wParam)
    {
      case VK_ESCAPE:
        OnClose();
        break;
      default:
        return CNetscapePropertySheet::PreTranslateMessage(pMsg);
        break;
    }
    return TRUE;
  }
  return CNetscapePropertySheet::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(CAdvancedOptionsPropertySheet, CNetscapePropertySheet)
#ifndef _WIN32
    ON_WM_CREATE()
#endif
    ON_WM_CLOSE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()
