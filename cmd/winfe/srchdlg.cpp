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
// srchdlg.cpp : implementation file
//

#include "stdafx.h"
#include "srchdlg.h"
#include "template.h"
#include "wfemsg.h"
#include "msg_srch.h"
#include "dirprefs.h"
#include "nethelp.h"
#include "xp_help.h"
#include "prefapi.h"
#include "intlwin.h"
#include "xp_time.h"
#include "xplocale.h"
#include "dateedit.h"
#include "intl_csi.h"


////////////////////////////////////////////////////////////////////////////
// CAddrEditProperities

CSearchDialog::CSearchDialog (LPCTSTR lpszCaption, MSG_Pane* pSearchPane, DIR_Server* pServer, CWnd * parent,
		 UINT numButtons, ButtonPosition buttonPosition, CPtrArray* buttonLabels) 
#ifdef FEATURE_BUTTONPROPERTYPAGE   
 :CButtonPropertySheet ( lpszCaption, parent, numButtons, buttonPosition, buttonLabels )
#endif
{
	m_pServer = pServer;
	m_pSearchPane = pSearchPane;
#ifdef FEATURE_BUTTONPROPERTYPAGE
	m_pBasicSearch = new CBasicSearch (this);
	AddPage( m_pBasicSearch );
	m_pAdvancedSearch = new CAdvancedSearch (this);
	AddPage( m_pAdvancedSearch );
#endif
}

CSearchDialog::CSearchDialog (UINT nIDCaption, MSG_Pane* pSearchPane, DIR_Server* pServer, CWnd * parent,
		 UINT numButtons, ButtonPosition buttonPosition, CUIntArray* buttonLabels) 
#ifdef FEATURE_BUTTONPROPERTYPAGE
    :CButtonPropertySheet ( nIDCaption, parent, numButtons, buttonPosition, buttonLabels )
#endif
{
	m_pServer = pServer;
	m_pSearchPane = pSearchPane;
#ifdef FEATURE_BUTTONPROPERTYPAGE
	m_pBasicSearch = new CBasicSearch (this);
	AddPage( m_pBasicSearch );
	m_pAdvancedSearch = new CAdvancedSearch (this);
	AddPage( m_pAdvancedSearch );
#endif
}

CSearchDialog::~CSearchDialog ( )
{

}

void CSearchDialog::PostNcDestroy( )
{
	if ( m_pAdvancedSearch )
		delete m_pAdvancedSearch;
	if ( m_pBasicSearch )
		delete m_pBasicSearch;
}

void CSearchDialog::OnHelp()
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	if (GetActivePage() == m_pAdvancedSearch)
		NetHelp(HELP_ADD_USER_PROPS);
	if (GetActivePage() == m_pBasicSearch)
		NetHelp(HELP_ADD_USER_PROPS);
#endif
}

#ifdef FEATURE_BUTTONPROPERTYPAGE
BEGIN_MESSAGE_MAP(CSearchDialog, CButtonPropertySheet)
#else
BEGIN_MESSAGE_MAP(CSearchDialog, CWnd)
#endif
	//{{AFX_MSG_MAP(CSearchDialog)
		// NOTE: the ClassWizard will add message map macros here
	    ON_WM_CREATE()
		ON_MESSAGE(LDS_GETSERVER,OnGetServer)
		ON_MESSAGE(LDS_GETSEARCHPANE,OnGetSearchPane)
		ON_MESSAGE(LDS_RECALC_LAYOUT,OnRecalcLayout)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


LRESULT CSearchDialog::OnGetServer(WPARAM wParam, LPARAM lParam)
{
	DIR_Server ** server = (DIR_Server **) lParam;
	(*server) = m_pServer;
	return 1;
}

LRESULT CSearchDialog::OnGetSearchPane(WPARAM wParam, LPARAM lParam)
{
	MSG_Pane ** pane = (MSG_Pane **) lParam;
	(*pane) = m_pSearchPane;
	return 1;
}

LRESULT CSearchDialog::OnRecalcLayout(WPARAM wParam, LPARAM lParam)
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	RecalcLayout();
#endif
	return 1;
}

int CSearchDialog::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	if (CButtonPropertySheet::OnCreate(lpCreateStruct) == -1)
#else
	if (CWnd::OnCreate(lpCreateStruct) == -1)
#endif
		return -1;

	int16 guicsid = 0;
	CString cs;

	guicsid = CIntlWin::GetSystemLocaleCsid();

	HDC hDC = ::GetDC(m_hWnd);
    LOGFONT lf;  
    memset(&lf,0,sizeof(LOGFONT));

	lf.lfPitchAndFamily = FF_SWISS;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = IntlGetLfCharset(CIntlWin::GetSystemLocaleCsid());
	if (CIntlWin::GetSystemLocaleCsid() == CS_LATIN1)
 		_tcscpy(lf.lfFaceName, "MS Sans Serif");
	else
 		_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(CIntlWin::GetSystemLocaleCsid()));
	lf.lfHeight = -MulDiv(9, ::GetDeviceCaps(hDC, LOGPIXELSY), 72);
	m_pFont = theApp.CreateAppFont( lf );

    ::ReleaseDC(m_hWnd,hDC);

	XP_Bool prefBool= FALSE;

	PREF_GetBoolPref("mail.addr_book.useAdvancedSearch", &prefBool);
	if (prefBool) {
#ifdef FEATURE_BUTTONPROPERTYPAGE
		SetActivePage (1);
#endif
		cs.LoadString (IDS_BASICSEARCH);
		GetDlgItem(IDC_BUTTON4)->SetWindowText(cs);
		cs.LoadString (IDS_ADVSEARCH_TITLE);
		SetWindowText (cs);
		((CAdvancedSearch*)m_pAdvancedSearch)->InitializePrevSearch ();
	}
	else {
#ifdef FEATURE_BUTTONPROPERTYPAGE
		SetActivePage (0);
#endif
		cs.LoadString (IDS_BASICSEARCH_TITLE);
		SetWindowText (cs);
		((CBasicSearch*)m_pBasicSearch)->InitializeSearchValues ();
	}

	return 0;
}

void CSearchDialog::OnButton2()
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	if (GetActiveIndex() == 0) {
		// save basic search
		((CBasicSearch*)m_pBasicSearch)->SavePreviousSearch();
	}
	else {
		// save advanced search
		((CAdvancedSearch*)m_pAdvancedSearch)->SavePreviousSearch();
	}


	CButtonPropertySheet::OnButton2();
#endif

} // END OF	FUNCTION CSearchDialog::OnButton2()

void CSearchDialog::OnButton3()
{
	OnHelp();

} // END OF	FUNCTION CSearchDialog::OnButton3()

void CSearchDialog::OnButton4()
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	CString cs;
	if (GetActiveIndex() == 0) {
		// switch to advanced
		cs.LoadString (IDS_BASICSEARCH);
		GetDlgItem(IDC_BUTTON4)->SetWindowText(cs);
		((CBasicSearch*)m_pBasicSearch)->SavePreviousSearch();
		SetActivePage(1);
		cs.LoadString (IDS_ADVSEARCH_TITLE);
		SetWindowText (cs);
		((CAdvancedSearch*)m_pAdvancedSearch)->InitializePrevSearch ();
	}
	else {
		// switch to basic
		cs.LoadString (IDS_ADVSEARCH);
		GetDlgItem(IDC_BUTTON4)->SetWindowText(cs);
		((CAdvancedSearch*)m_pAdvancedSearch)->SavePreviousSearch();
		SetActivePage(0);
		cs.LoadString (IDS_BASICSEARCH_TITLE);
		SetWindowText (cs);
		((CBasicSearch*)m_pBasicSearch)->InitializeSearchValues ();
	}
	PREF_SetBoolPref("mail.addr_book.useAdvancedSearch", GetActiveIndex());
#endif
} // END OF	FUNCTION CSearchDialog::OnButton4()


/////////////////////////////////////////////////////////////////////////////
// CAdvancedSearch property page

CAdvancedSearch::CAdvancedSearch(CWnd *pParent) 
#ifdef FEATURE_BUTTONPROPERTYPAGE
	: CButtonPropertyPage(CAdvancedSearch::IDD)
#endif
{
	//{{AFX_DATA_INIT(CAddressUser)
	//}}AFX_DATA_INIT
	m_iMoreCount = 0;
	m_bLogicType = 0;
	m_bChanged = FALSE;
}

CAdvancedSearch::~CAdvancedSearch()
{
}

void CAdvancedSearch::DoDataExchange(CDataExchange* pDX)
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	CButtonPropertyPage::DoDataExchange(pDX);
#else
	CDialog::DoDataExchange(pDX);
#endif

	//{{AFX_DATA_MAP(CAdvancedSearch)
	//}}AFX_DATA_MAP
}

#ifdef FEATURE_BUTTONPROPERTYPAGE
BEGIN_MESSAGE_MAP(CAdvancedSearch, CButtonPropertyPage)
#else
BEGIN_MESSAGE_MAP(CAdvancedSearch, CDialog)
#endif
	//{{AFX_MSG_MAP(CAdvancedSearch)
		// NOTE: the ClassWizard will add message map macros here
	ON_BN_CLICKED(IDC_MORE, OnMore)
	ON_BN_CLICKED(IDC_FEWER, OnFewer)
	ON_BN_CLICKED(IDC_CLEAR_SEARCH, OnClearSearch)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB1, OnAttrib1)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB2, OnAttrib2)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB3, OnAttrib3)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB4, OnAttrib4)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB5, OnAttrib5)
	ON_CBN_SELCHANGE(IDC_COMBO_OP1, OnOperatorValueChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_OP2, OnOperatorValueChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_OP3, OnOperatorValueChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_OP4, OnOperatorValueChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_OP5, OnOperatorValueChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_AND_OR, OnAndOr)
	ON_EN_CHANGE( IDC_EDIT_VALUE1, OnEditValueChanged )
	ON_EN_CHANGE( IDC_EDIT_VALUE2, OnEditValueChanged )
	ON_EN_CHANGE( IDC_EDIT_VALUE3, OnEditValueChanged )
	ON_EN_CHANGE( IDC_EDIT_VALUE4, OnEditValueChanged )
#ifdef ON_UPDATE_COMMAND_UI_RANGE
	ON_UPDATE_COMMAND_UI_RANGE( IDC_COMBO_ATTRIB1, IDC_EDIT_VALUE5, OnUpdateQuery )
#endif
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


#ifndef ON_UPDATE_COMMAND_UI_RANGE

BOOL CAdvancedSearch::OnCmdMsg( UINT nID, int nCode, void* pExtra, 
							 AFX_CMDHANDLERINFO* pHandlerInfo ) 
{
	if ((nID >= IDC_COMBO_ATTRIB1) && (nID <= IDC_EDIT_VALUE5) && 
		( nCode == CN_UPDATE_COMMAND_UI) ) {
		OnUpdateQuery( (CCmdUI *) pExtra );
		return TRUE;
	}
	return CDialog::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );
}

#endif

void CAdvancedSearch::OnUpdateQuery( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( TRUE );
}

void CAdvancedSearch::AdjustHeight(int dy)
{
	CRect rect;
	GetWindowRect(&rect);

	CSize size = rect.Size();
	size.cy += dy;

	SetWindowPos( NULL, 0, 0, size.cx, size.cy, SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW);
	GetWindowRect(&rect);
}


/////////////////////////////////////////////////////////////////////////////
// CAddressUser message handlers

BOOL CAdvancedSearch::OnInitDialog() 
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	// TODO: Add your specialized code here and/or call the base class
	CButtonPropertyPage::OnInitDialog();
#endif
	m_searchObj.InitializeAttributes (widgetText, attribCommonName);

	DIR_Server * pServer = NULL;
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &pServer);

	m_searchObj.New (this);

	m_searchObj.UpdateAttribList(scopeLdapDirectory, pServer);
	m_searchObj.UpdateOpList(scopeLdapDirectory, pServer);

	GetDlgItem(IDC_STATIC1)->SetWindowText(pServer->description);

	return TRUE;
}


void CAdvancedSearch::InitializePrevSearch() 
{
	int dy = 0;
	MSG_Pane * pane = NULL;
	DIR_Server * pServer = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &pServer);
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);
	
	dy += m_searchObj.InitializeLDAPSearchWindow (pane, pServer, &m_iMoreCount, m_bLogicType);

	if (m_iMoreCount < 4)
		GetDlgItem(IDC_MORE)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_MORE)->EnableWindow (FALSE);
	if (m_iMoreCount > 0)
		GetDlgItem(IDC_FEWER)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_FEWER)->EnableWindow (FALSE);

	AdjustHeight (dy);

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_RECALC_LAYOUT, 0, 0);
}


BOOL CAdvancedSearch::SavePreviousSearch( )
{
	// save the query 
	if (m_bChanged) 
	{
		MSG_Pane * pane = NULL;
		DIR_Server * server = NULL;

		::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
		::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);
		MSG_SearchFree (pane);
		MSG_SearchAlloc (pane);
		MSG_AddLdapScope(pane, server);
		m_searchObj.BuildQuery(pane, m_iMoreCount, m_bLogicType);
	}

	return (TRUE);
}


BOOL CAdvancedSearch::OnSetActive() 
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	// TODO: Add your specialized code here and/or call the base class
	if(!CButtonPropertyPage::OnSetActive()) 
        return(FALSE);

	m_bChanged = FALSE;
#endif
	return(TRUE);
}


void CAdvancedSearch::OnOK()
{
	MSG_Pane * pane = NULL;
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);
	MSG_SearchFree (pane);
	MSG_SearchAlloc (pane);
	MSG_AddLdapScope(pane, server);
	m_searchObj.BuildQuery(pane, m_iMoreCount, m_bLogicType);
}

void CAdvancedSearch::OnAndOr()
{
	m_searchObj.OnAndOr(m_iMoreCount, &m_bLogicType);
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnMore()
{
	int dy = 0;

	dy = m_searchObj.More(&m_iMoreCount, m_bLogicType);

	if (m_iMoreCount < 4)
		GetDlgItem(IDC_MORE)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_MORE)->EnableWindow (FALSE);
	if (m_iMoreCount > 0)
		GetDlgItem(IDC_FEWER)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_FEWER)->EnableWindow (FALSE);

	AdjustHeight (dy);

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_RECALC_LAYOUT, 0, 0);
}

void CAdvancedSearch::OnFewer()
{
	int dy = 0;

	dy = m_searchObj.Fewer(&m_iMoreCount, m_bLogicType);

	if (m_iMoreCount < 4)
		GetDlgItem(IDC_MORE)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_MORE)->EnableWindow (FALSE);
	if (m_iMoreCount > 0)
		GetDlgItem(IDC_FEWER)->EnableWindow (TRUE);
	else
		GetDlgItem(IDC_FEWER)->EnableWindow (FALSE);

	AdjustHeight (dy);

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_RECALC_LAYOUT, 0, 0);
}

void CAdvancedSearch::OnClearSearch()
{
	int dy = 0;

	MSG_Pane * pane = NULL;
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);

	MSG_SearchFree (pane);

	dy = m_searchObj.ClearSearch(&m_iMoreCount, TRUE);

	AdjustHeight (dy);
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_RECALC_LAYOUT, 0, 0);
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnEditValueChanged ()
{
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnOperatorValueChanged ()
{
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnAttrib1()
{
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);

	m_searchObj.UpdateOpList(0, scopeLdapDirectory, server);
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnAttrib2()
{
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
	m_searchObj.UpdateOpList(1, scopeLdapDirectory, server);
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnAttrib3()
{
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
	m_searchObj.UpdateOpList(2, scopeLdapDirectory, server);
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnAttrib4()
{
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
	m_searchObj.UpdateOpList(3, scopeLdapDirectory, server);
	m_bChanged = TRUE;
}

void CAdvancedSearch::OnAttrib5()
{
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
	m_searchObj.UpdateOpList(4, scopeLdapDirectory, server);
	m_bChanged = TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CBasicSearch property page

static int BasicChoicesTable[][2] =
	{{IDC_STATIC1, IDC_EDIT_VALUE1},
	 {IDC_STATIC2, IDC_EDIT_VALUE2},
	 {IDC_STATIC3, IDC_EDIT_VALUE3},
	 {IDC_STATIC4, IDC_EDIT_VALUE4}};

#define BASIC_COL_LABEL		0
#define BASIC_COL_VALUE		1
#define BASIC_COL_COUNT		2

static _TCHAR szResultText[64];

CBasicSearch::CBasicSearch(CWnd *pParent) 
#ifdef FEATURE_BUTTONPROPERTYPAGE
	: CButtonPropertyPage(CBasicSearch::IDD)
#endif
{
	//{{AFX_DATA_INIT(CBasicSearch)
	//}}AFX_DATA_INIT
	m_bChanged = FALSE;
}

CBasicSearch::~CBasicSearch()
{
	m_bLogicType = 1;
}

void CBasicSearch::DoDataExchange(CDataExchange* pDX)
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	CButtonPropertyPage::DoDataExchange(pDX);
#else
	CDialog::DoDataExchange(pDX);
#endif
	//{{AFX_DATA_MAP(CBasicSearch)
	//}}AFX_DATA_MAP
}

#ifdef FEATURE_BUTTONPROPERTYPAGE
BEGIN_MESSAGE_MAP(CBasicSearch, CButtonPropertyPage)
#else
BEGIN_MESSAGE_MAP(CBasicSearch, CDialog)
#endif

	//{{AFX_MSG_MAP(CBasicSearch)
		// NOTE: the ClassWizard will add message map macros here
	ON_EN_CHANGE( IDC_EDIT_VALUE1, OnEditValueChanged )
	ON_EN_CHANGE( IDC_EDIT_VALUE2, OnEditValueChanged )
	ON_EN_CHANGE( IDC_EDIT_VALUE3, OnEditValueChanged )
	ON_EN_CHANGE( IDC_EDIT_VALUE4, OnEditValueChanged )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddressUser message handlers

BOOL CBasicSearch::OnInitDialog() 
{
	MSG_SearchMenuItem items[4];
	int maxItems = 4;

#ifdef FEATURE_BUTTONPROPERTYPAGE
	CButtonPropertyPage::OnInitDialog();
#endif

	DIR_Server * pServer = NULL;
	int j = 0;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &pServer);
	GetDlgItem(IDC_STATIC5)->SetWindowText(pServer->description);

	MSG_GetBasicLdapSearchAttributes (pServer, items, &maxItems);

	for (j = 0; j < maxItems; j++) {
		(GetDlgItem (BasicChoicesTable[j][BASIC_COL_LABEL]))->SetWindowText (items[j].name);
	}
	
	return TRUE;
}

void CBasicSearch::InitializeSearchValues ()
{
	MSG_Pane * pane = NULL;
	int numTerms = 0;
	MSG_SearchAttribute attrib;
	MSG_SearchOperator op;
	MSG_SearchValue value;
	MSG_SearchMenuItem items[4];
	int maxItems = 4;
	int j = 0;
	DIR_Server * pServer = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &pServer);
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);

	MSG_GetBasicLdapSearchAttributes (pServer, items, &maxItems);
	MSG_CountSearchTerms (pane, &numTerms);

	// set all the terms back to empty strings
	for (j = 0; j < maxItems; j++) {
		(GetDlgItem (BasicChoicesTable[j][BASIC_COL_VALUE]))->SetWindowText("");
	}

	// get the attributes for the directory used in the last search
	// and make sure that 
	// it hasn't been remaped from customization 
	int numScopes = 0;
	DIR_Server * server = NULL;
	uint16 maxItems2 = 16;
	MSG_SearchMenuItem items2[16];
	MSG_ScopeAttribute scope = scopeLdapDirectory;
	MSG_CountSearchScopes (pane, &numScopes);
	BOOL bCompareAttrib = FALSE;
			
	// find the attributes
	if (numScopes) {
		ASSERT (numScopes == 1);
		MSG_GetNthSearchScope (pane, 0, &scope, (void**) &server);
		if (server && !DIR_AreServersSame (pServer, server)) {					
			MSG_GetAttributesForSearchScopes (WFE_MSGGetMaster(), scopeLdapDirectory, 
						(void**) &server, 1, items2, &maxItems2);
			bCompareAttrib = TRUE;
		}
	}
				
	// set them to the values in the search pane
	for (int i = 0; i < maxItems && i < numTerms; i++)
	{
		BOOL found = FALSE;
		int k = 0;
		MSG_GetNthSearchTerm (pane, i, &attrib, &op, &value);
		if (bCompareAttrib)
		{
			while (k < maxItems2 && !found) {
				if (items2 [k].attrib != attrib)
					k++;
				else 
					found = TRUE;
			}
		}

		for (j = 0; j < maxItems; j++) {
			if ((items[j].attrib ==  attrib) && (!(GetDlgItem (BasicChoicesTable[j][BASIC_COL_VALUE]))->GetWindowTextLength())) {
				if (bCompareAttrib) {
					if (found && (strcmp (items2 [k].name, items[j].name ) == 0))
						(GetDlgItem (BasicChoicesTable[j][BASIC_COL_VALUE]))->SetWindowText(value.u.string);
				}
				else
					(GetDlgItem (BasicChoicesTable[j][BASIC_COL_VALUE]))->SetWindowText(value.u.string);
			}
		}
	}
}

void CBasicSearch::OnEditValueChanged ()
{
	m_bChanged = TRUE;
}


BOOL CBasicSearch::SavePreviousSearch( )
{
	// save the query 
	if (m_bChanged) 
	{
		MSG_Pane * pane = NULL;
		DIR_Server * server = NULL;

		::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
		::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);
		MSG_SearchFree (pane);
		MSG_SearchAlloc (pane);
		MSG_AddLdapScope(pane, server);
		BuildQuery(pane, m_bLogicType);
	}

	return (TRUE);
}


BOOL CBasicSearch::OnSetActive() 
{
#ifdef FEATURE_BUTTONPROPERTYPAGE
	if(!CButtonPropertyPage::OnSetActive()) 
        return(FALSE);

	m_bChanged = FALSE;
#endif

	return(TRUE);
}


void CBasicSearch::OnOK()
{
	MSG_Pane * pane = NULL;
	DIR_Server * server = NULL;

	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &server);
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSEARCHPANE, 0, (LPARAM) &pane);
	MSG_SearchFree (pane);
	MSG_SearchAlloc (pane);
	MSG_AddLdapScope(pane, server);
	BuildQuery(pane, m_bLogicType);
}

void CBasicSearch::BuildQuery (MSG_Pane* searchPane, BOOL bLogicType)
{
	MSG_SearchAttribute attrib;
	MSG_SearchOperator op;
	MSG_SearchValue value;
	CNSDateEdit *date = NULL;
	CComboBox * combo = NULL;
	int iCurSel = 0;
	CWnd * widget = NULL;
	MSG_SearchMenuItem items[4];
	int maxItems = 4;

	DIR_Server * pServer = NULL;
	::SendMessage (GetParent()->GetSafeHwnd(), LDS_GETSERVER, 0, (LPARAM) &pServer);

	MSG_GetBasicLdapSearchAttributes (pServer, items, &maxItems);

	for (int i = 0; i <= 3; i++) {
		attrib = (MSG_SearchAttribute) items[i].attrib;

		op = (MSG_SearchOperator) opContains;

		widget = GetDlgItem(BasicChoicesTable[i][BASIC_COL_VALUE]);
		widget->GetWindowText(szResultText, sizeof(szResultText));

		value.attribute = attrib;
		switch (attrib) {
		case attribDate: 
			{
				CTime ctime;
				date = (CNSDateEdit *) GetDlgItem(BasicChoicesTable[i][BASIC_COL_VALUE]);
				date->GetDate(ctime);
				value.u.date = ctime.GetTime();
			}
			break;
		case attribPriority:
			combo = (CComboBox *) GetDlgItem(BasicChoicesTable[i][BASIC_COL_VALUE]);
			iCurSel = combo->GetCurSel();
			value.u.priority = (MSG_PRIORITY) combo->GetItemData(iCurSel);
			break;
		case attribMsgStatus:
			combo = (CComboBox *) GetDlgItem(BasicChoicesTable[i][BASIC_COL_VALUE]);
			iCurSel = combo->GetCurSel();
			value.u.msgStatus = combo->GetItemData(iCurSel);
			break;
		default:
			if (XP_STRLEN (szResultText))
				value.u.string = XP_STRDUP (szResultText);
			else
				value.u.string = NULL;
		}
		if (value.u.string)
		{
			MSG_AddSearchTerm(searchPane, attrib, op, &value, !bLogicType, NULL);
			XP_FREE(value.u.string);
		}
	}
	
}


