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
// filter.cpp : implementation file
//

#include "stdafx.h"
#include "filter.h"
#include "mailmisc.h"
#include "xp_time.h"
#include "xplocale.h"
#include "wfemsg.h"
#include "dateedit.h"
#include "nethelp.h"
#include "xp_help.h"
#include "edhdrdlg.h"
#include "prefapi.h"
#include "numedit.h"
#include "mailpriv.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern MSG_SearchError MSG_GetValuesForAttribute( MSG_ScopeAttribute scope, 
												  MSG_SearchAttribute attrib, 
												  MSG_SearchMenuItem *items, 
												  uint16 *maxItems);


/////////////////////////////////////////////////////////////////////////////
// CFilterList

CFilterList::CFilterList(): CListBox()
{
	m_iPosIndex = 0;
	m_iPosName = 50;
	m_iPosStatus = 100;

    ApiApiPtr(api);
    m_pIImageUnk = api->CreateClassInstance(APICLASS_IMAGEMAP);
	ASSERT(m_pIImageUnk);
    if (m_pIImageUnk) {
        m_pIImageUnk->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImageMap);
        ASSERT(m_pIImageMap);
        m_pIImageMap->Initialize(IDB_MAILNEWS,16,16);
    }
}

CFilterList::~CFilterList()
{
	if (m_pIImageUnk && m_pIImageMap ) {
		m_pIImageUnk->Release();
	}
}

void CFilterList::SetColumnPositions(int iPosIndex, int iPosName, int iPosStatus)
{
	m_iPosIndex = iPosIndex;
	m_iPosName = iPosName;
	m_iPosStatus = iPosStatus;
}

BEGIN_MESSAGE_MAP( CFilterList, CListBox )
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP( )
 
void CFilterList::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	HDC hDC = lpDrawItemStruct->hDC;
	RECT rcItem = lpDrawItemStruct->rcItem;
	MSG_Filter *itemData = (MSG_Filter *) lpDrawItemStruct->itemData;
	HBRUSH hBrushFill = NULL;
	COLORREF oldBk, oldText;

	if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
		hBrushFill = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
		oldBk = ::SetBkColor( hDC, GetSysColor( COLOR_HIGHLIGHT ) );
		oldText = ::SetTextColor( hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
	} else {
		hBrushFill = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
		oldBk = ::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		oldText = ::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
	}

	if ( itemData ) {
		RECT rcTemp = rcItem;
		_TCHAR lpszIndex[8]; // Temp string for index
		LPSTR lpszName;
		XP_Bool bEnabled;

		::FillRect( hDC, &rcItem, hBrushFill );

		_stprintf( lpszIndex, _T("%3d"), lpDrawItemStruct->itemID + 1);
		MSG_GetFilterName( itemData, (char **) &lpszName );
		MSG_IsFilterEnabled( itemData, &bEnabled );

		rcTemp.left = m_iPosIndex;
		rcTemp.right = m_iPosName;
		::DrawText( hDC, lpszIndex, -1, &rcTemp, DT_VCENTER|DT_LEFT|DT_NOPREFIX );
		rcTemp.left = rcTemp.right;
		rcTemp.right = m_iPosStatus;
		::DrawText( hDC, lpszName, -1, 
					&rcTemp, DT_VCENTER|DT_LEFT );
        m_pIImageMap->DrawTransImage( bEnabled ? IDX_CHECKMARK : IDX_CHECKBOX,
									  m_iPosStatus, rcItem.top, hDC );
	}
	if ( lpDrawItemStruct->itemAction & ODA_FOCUS ) {
		::DrawFocusRect( hDC, &lpDrawItemStruct->rcItem );
	}

	if (hBrushFill)
		VERIFY( ::DeleteObject( hBrushFill ) );

	::SetBkColor( hDC, oldBk );
	::SetTextColor( hDC, oldText ); 
}

UINT CFilterList::ItemFromPoint(CPoint pt, BOOL& bOutside) const
{
	RECT rect;
	GetClientRect(&rect);

	int iHeight = GetItemHeight(0);
	int iCount = GetCount();
	int iTopIndex = GetTopIndex();

	int iListHeight = iHeight * ( iCount - iTopIndex );
	rect.bottom = rect.bottom < iListHeight ? rect.bottom : iListHeight;

	bOutside = !::PtInRect(&rect, pt);

	if ( bOutside ) {
		return 0;
	} 

	return (pt.y / iHeight) + iTopIndex; 
}

void CFilterList::OnLButtonDown( UINT nFlags, CPoint point )
{
	if ( point.x > m_iPosStatus ) {
		BOOL bOutside;
		UINT item = ItemFromPoint( point, bOutside );
		RECT rcItem;
		GetItemRect( item, &rcItem );

		if ( (point.y < rcItem.bottom) && (point.y >= rcItem.top) ) {
			MSG_Filter *pFilter = (MSG_Filter *) GetItemData( item );
			if ( pFilter ) {
				XP_Bool bEnabled;
				MSG_IsFilterEnabled( pFilter, &bEnabled );
				MSG_EnableFilter( pFilter, !bEnabled  );
				InvalidateRect( &rcItem );
			}
		}
	} else {
		CListBox::OnLButtonDown( nFlags, point );
	}
}

void CFilterList::OnLButtonDblClk( UINT nFlags, CPoint point )
{
	if ( point.x > m_iPosStatus ) {
		BOOL bOutside;
		UINT item = ItemFromPoint( point, bOutside );
		RECT rcItem;
		GetItemRect( item, &rcItem );

		if ( (point.y < rcItem.bottom) && (point.y >= rcItem.top) ) {
			MSG_Filter *pFilter = (MSG_Filter *) GetItemData( item );
			if ( pFilter ) {
				XP_Bool bEnabled;
				MSG_IsFilterEnabled( pFilter, &bEnabled );
				MSG_EnableFilter( pFilter, !bEnabled  );
				InvalidateRect( &rcItem );
			}
		}
	} else {
		CListBox::OnLButtonDblClk( nFlags, point );
	}
}

/////////////////////////////////////////////////////////////////////////////
// CFilterPickerDialog dialog

#define COL_COUNT	5
#define COL_ATTRIB	0
#define COL_OP		1
#define COL_VALUE	2

static int RuleMatrix[5][COL_COUNT] = {
	{ IDC_COMBO_ATTRIB1, IDC_COMBO_OP1, IDC_EDIT_VALUE1, IDC_STATIC_THE1, IDC_STATIC_OF1 },
	{ IDC_COMBO_ATTRIB2, IDC_COMBO_OP2, IDC_EDIT_VALUE2, IDC_STATIC_THE2, IDC_STATIC_OF2 },
	{ IDC_COMBO_ATTRIB3, IDC_COMBO_OP3, IDC_EDIT_VALUE3, IDC_STATIC_THE3, IDC_STATIC_OF3 },
	{ IDC_COMBO_ATTRIB4, IDC_COMBO_OP4, IDC_EDIT_VALUE4, IDC_STATIC_THE4, IDC_STATIC_OF4 },
	{ IDC_COMBO_ATTRIB5, IDC_COMBO_OP5, IDC_EDIT_VALUE5, IDC_STATIC_THE5, IDC_STATIC_OF5 }
};

static MSG_SearchValueWidget WidgetWas[] = 
	{ widgetText, 
	  widgetText, 
	  widgetText,
	  widgetText,
	  widgetText };

static MSG_SearchAttribute AttribWas[] =
	{ attribSender,
	  attribSender,
	  attribSender,
	  attribSender,
	  attribSender };

CFilterPickerDialog::CFilterPickerDialog(MSG_Pane *pPane, CWnd* pParent /*=NULL*/)
	: CDialog(CFilterPickerDialog::IDD, pParent)
{
	m_pPane = pPane;
}


void CFilterPickerDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

int CFilterPickerDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_FilterList.SubclassDlgItem( IDC_LIST_FILTERS, this );

	m_UpButton.AutoLoad( IDC_UP, this );
	m_DownButton.AutoLoad( IDC_DOWN, this );

	MSG_OpenFolderFilterList(m_pPane, NULL, filterInbox, &m_pFilterList);
	MSG_GetFilterCount(m_pFilterList, &m_iFilterCount);
	m_bLoggingEnabled = MSG_IsLoggingEnabled(m_pFilterList);
	m_iIndex = 0;

	CWnd *widget;
    widget = GetDlgItem(IDC_CHECK_LOG);
	((CButton *) widget)->SetCheck(m_bLoggingEnabled);

#ifdef _WIN32
	if ( sysInfo.m_bWin4 ) {
		widget = GetDlgItem( IDC_STATIC_DESCRIPTION );
		widget->ModifyStyle( WS_BORDER, 0, SWP_FRAMECHANGED );
		widget->ModifyStyleEx( 0, WS_EX_CLIENTEDGE, SWP_FRAMECHANGED );
	}
#endif

	int iPosIndex, iPosName, iPosStatus;

	RECT rect, rcText;
	
	widget = GetDlgItem(IDC_LIST_FILTERS);
	widget->GetWindowRect(&rect);
	int iListLeft = 4;

	CDC *pDC = GetDC();
	::SetRect( &rcText, 0, 0, 64, 64 );
	pDC->DrawText(_T("0"), 1, &rcText, DT_CALCRECT ); 
	ReleaseDC( pDC );

	iPosIndex = iListLeft + 4;
	iPosName = iPosIndex + rcText.right * 3;
	iPosStatus = rect.right - rect.left - 20 - GetSystemMetrics(SM_CXVSCROLL);

	m_FilterList.SetColumnPositions( iPosIndex, iPosName, iPosStatus );

	UpdateList();
	
	return TRUE;
}

void CFilterPickerDialog::UpdateList()
{
	MSG_GetFilterCount(m_pFilterList, &m_iFilterCount);

	const char *lpszName;
	CListBox *pListBox = (CListBox *) GetDlgItem( IDC_LIST_FILTERS );
	pListBox->ResetContent();

	for ( int i = 0; i < m_iFilterCount; i++ ) {
		if (FilterError_Success == MSG_GetFilterAt(m_pFilterList, (MSG_FilterIndex) i, &m_pFilter) && m_pFilter)
		{
			MSG_GetFilterName( m_pFilter, (char **) &lpszName );
			pListBox->AddString( lpszName );
			pListBox->SetItemData( i, (ULONG) m_pFilter );
		}
	}
	pListBox->SetCurSel(m_iIndex);

	OnSelchangeListFilter();
}

BEGIN_MESSAGE_MAP(CFilterPickerDialog, CDialog)
	ON_BN_CLICKED( IDOK, OnOK)
	ON_BN_CLICKED( IDCANCEL, OnCancel)
	ON_BN_CLICKED( IDC_NEW, OnNew)
	ON_BN_CLICKED( IDC_EDIT, OnEdit)
	ON_UPDATE_COMMAND_UI( IDC_EDIT, OnUpdateEdit )
	ON_BN_CLICKED( IDC_DELETE, OnDelete)
	ON_UPDATE_COMMAND_UI( IDC_DELETE, OnUpdateEdit )
	ON_BN_CLICKED( IDC_UP, OnUp)
	ON_BN_DOUBLECLICKED( IDC_UP, OnUp)
	ON_UPDATE_COMMAND_UI( IDC_UP, OnUpdateUp )
	ON_BN_CLICKED( IDC_DOWN, OnDown)
	ON_BN_DOUBLECLICKED( IDC_DOWN, OnDown)
	ON_UPDATE_COMMAND_UI( IDC_DOWN, OnUpdateDown )
	ON_LBN_DBLCLK( IDC_LIST_FILTERS, OnDblclkListFilter )
	ON_LBN_SELCHANGE( IDC_LIST_FILTERS, OnSelchangeListFilter )
	ON_BN_CLICKED( IDC_FILTER_HELP, OnHelp)
	ON_BN_CLICKED( IDC_VIEW, OnView)
END_MESSAGE_MAP()

void CFilterPickerDialog::OnDblclkListFilter() 
{
	if (m_iIndex < m_iFilterCount) {
		OnEdit();
	} else {
		OnNew();
	}
}

void CFilterPickerDialog::OnSelchangeListFilter() 
{
	CListBox *pListBox = (CListBox *) GetDlgItem( IDC_LIST_FILTERS );

	m_iIndex = pListBox->GetCurSel();

	CWnd *widget;

	m_pFilter = NULL;
	MSG_GetFilterAt( m_pFilterList, (MSG_FilterIndex) m_iIndex, &m_pFilter );

	const char *lpszDesc = "";
	widget = GetDlgItem(IDC_STATIC_DESCRIPTION);
	if ( m_pFilter ) {
		MSG_GetFilterDesc( m_pFilter, (char **) &lpszDesc );
	}
	widget->SetWindowText( lpszDesc );

	UpdateDialogControls( this, TRUE );
}

void CFilterPickerDialog::OnSelcancelListFilter() 
{
}

void CFilterPickerDialog::OnNew() 
{
	m_pFilter = NULL;
	CFilterDialog dialog( m_pPane, &m_pFilter, this );

	if (dialog.DoModal() == IDOK ) {
		m_iIndex += 1;
		MSG_InsertFilterAt( m_pFilterList, (MSG_FilterIndex) m_iIndex, m_pFilter );
	}

	UpdateList();
}

void CFilterPickerDialog::OnEdit() 
{
	MSG_GetFilterAt(m_pFilterList, (MSG_FilterIndex) m_iIndex, &m_pFilter);
	CFilterDialog dialog( m_pPane, &m_pFilter, this );

	if ( dialog.DoModal() == IDOK ) {
		MSG_SetFilterAt( m_pFilterList, (MSG_FilterIndex) m_iIndex, m_pFilter );
	}

	UpdateList();
}

void CFilterPickerDialog::OnUpdateEdit( CCmdUI *pCmdUI )
{
	BOOL bEnable = (m_iIndex < m_iFilterCount) && (m_iIndex >= 0);

    MSG_FilterType filterType = filterInboxJavaScript;
    MSG_GetFilterType( m_pFilter, &filterType );
    if ( filterType == filterInboxJavaScript || filterType == 
		filterNewsJavaScript ) {
		bEnable = FALSE;
	}

    pCmdUI->Enable(bEnable);
}

void CFilterPickerDialog::OnDelete() 
{
	if (m_iIndex < m_iFilterCount && m_iIndex >= 0) {
		MSG_GetFilterAt( m_pFilterList, (MSG_FilterIndex) m_iIndex, &m_pFilter );
		MSG_RemoveFilterAt( m_pFilterList, (MSG_FilterIndex) m_iIndex );
		MSG_DestroyFilter( m_pFilter );
		if (m_iIndex) {
			m_iIndex--;
		}
		UpdateList();
	}
}

void CFilterPickerDialog::OnUp() 
{
	MSG_MoveFilterAt(m_pFilterList, (MSG_FilterIndex) m_iIndex, filterUp);
	m_iIndex--;
	UpdateList();
}
	
void CFilterPickerDialog::OnUpdateUp( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( m_iIndex > 0 );
}

void CFilterPickerDialog::OnDown() 
{
	MSG_MoveFilterAt(m_pFilterList, (MSG_FilterIndex) m_iIndex, filterDown);
	m_iIndex++;
	UpdateList();
}

void CFilterPickerDialog::OnUpdateDown( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( m_iFilterCount > 0 && m_iIndex < m_iFilterCount - 1 );
}

void CFilterPickerDialog::OnOK() 
{
	CDialog::OnOK();
	MSG_EnableLogging(m_pFilterList, IsDlgButtonChecked(IDC_CHECK_LOG));

	MSG_CloseFilterList(m_pFilterList);	
}

void CFilterPickerDialog::OnCancel() 
{
	CDialog::OnCancel();
	MSG_CancelFilterList( m_pFilterList );
}

void CFilterPickerDialog::OnHelp() 
{
	NetHelp(HELP_MAIL_FILTERS);
}

void CFilterPickerDialog::OnView()
{
	MSG_ViewFilterLog(m_pPane);
}
/////////////////////////////////////////////////////////////////////////////
// CFilterDialog dialog


CFilterDialog::CFilterDialog(MSG_Pane *pPane, MSG_Filter **filter, 
							 CWnd* pParent /*=NULL*/)
	: CDialog(CFilterDialog::IDD, pParent)
{
	m_pPane = pPane;
	m_hFilter = filter;
	m_bLogicType = 0;

	m_iRowIndex = 0;
	m_pCustomHeadersDlg = NULL;

	//{{AFX_DATA_INIT(CFilterDialog)
	m_iActive = 1;
	m_lpszFilterName = szLoadString(IDS_UNTITLED);
	m_lpszFilterDesc = _T("");
	//}}AFX_DATA_INIT

	for (int i = 0; i < 5; i++ ) {
		WidgetWas[i] = widgetText;
		AttribWas[i] = attribSender;
	}
}


void CFilterDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFilterDialog)
	DDX_Radio(pDX, IDC_RADIO_OFF, m_iActive);
	DDX_Text(pDX, IDC_EDIT_NAME, m_lpszFilterName);
	DDX_Text(pDX, IDC_EDIT_DESC, m_lpszFilterDesc);
	//}}AFX_DATA_MAP
}

int CFilterDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	uint16 i, j;
	int dy;
	CWnd *widget;
	CRect rect, rect2;

	CComboBox *combo;

	MSG_FolderInfo *pInbox = NULL;
	MSG_GetFoldersWithFlag (WFE_MSGGetMaster(), MSG_FOLDER_FLAG_INBOX, &pInbox, 1);

	uint16 numItems;
	MSG_GetNumAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, &numItems); 
    MSG_SearchMenuItem * HeaderItems = new MSG_SearchMenuItem [numItems+1];
	
	if (!HeaderItems) 
		return FALSE;  //something bad happened here!!

	MSG_GetAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, HeaderItems, &numItems);
	CString strEditCustom;
	strEditCustom.LoadString(IDS_EDIT_CUSTOM);
	strcpy( HeaderItems[numItems].name,  strEditCustom);
	HeaderItems[numItems].attrib = -1;  //identifies a command to launch the Edit headers dialog
	HeaderItems[numItems].isEnabled = FALSE;
	BOOL bUsesCustomHeaders = MSG_ScopeUsesCustomHeaders(WFE_MSGGetMaster(), scopeMailFolder, pInbox, TRUE);


	for (i = 0; i < 5; i++) {

		combo = (CComboBox *) GetDlgItem( RuleMatrix[i][COL_ATTRIB] );
		combo->ResetContent();
		for (j = 0; j < numItems; j++) {
			combo->AddString(HeaderItems[j].name);
			combo->SetItemData(j, HeaderItems[j].attrib);
		}
		if (j == numItems && bUsesCustomHeaders)
		{   //place the edit text in the last position of the combo box
			combo->AddString(HeaderItems[j].name);
			combo->SetItemData(j, HeaderItems[j].attrib);
		}
		if (i > 0) {
			for (j = 0; j < COL_COUNT; j++) {
				widget = GetDlgItem(RuleMatrix[i][j]);
				widget->ShowWindow(SW_HIDE);
			}
		}
		combo->SetCurSel(0);
		UpdateOpList(i);
	}

	if (HeaderItems) 
		delete HeaderItems; //free the memory since it's now in the combo box

	//Above we are getting the list of Headers and updating controls in column 1 

	CComboBox *comboAndOr = (CComboBox*) GetDlgItem(IDC_COMBO_AND_OR);
	if (comboAndOr)
	{	//Default to AND logic for display in the combobox
		comboAndOr->SetCurSel(0);
	}


	MSG_RuleActionType type;
	combo = (CComboBox *) GetDlgItem( IDC_COMBO_ACTION );
	combo->ResetContent();

	MSG_RuleMenuItem items[16];
	uint16 maxItems = 16;

	MSG_GetRuleActionMenuItems(filterInbox, items, &maxItems);
	for (j = 0; j < maxItems; j++) {
		combo->AddString(items[j].name);
		combo->SetItemData(j, items[j].attrib);
	}
	combo->SetCurSel( 0 );

	UpdateDestList();

	widget = GetDlgItem(IDC_FEWER);
	widget->EnableWindow(FALSE);
	widget->GetWindowRect(&rect);
	widget = GetDlgItem(RuleMatrix[0][0]);
	widget->GetWindowRect(&rect2);
	dy = rect2.bottom - rect.top;
	widget = GetDlgItem(RuleMatrix[4][0]);
	widget->GetWindowRect(&rect2);
	dy += rect.top - rect2.bottom;

	SlideBottomControls(dy);

	if ( *m_hFilter ) {
		const char *lpszTemp;
		CWnd *widget;
		CButton *button;

		// Name
		MSG_GetFilterName( *m_hFilter, (char **) &lpszTemp);
		m_lpszFilterName = lpszTemp;
		widget = GetDlgItem( IDC_EDIT_NAME );
		widget->SetWindowText(lpszTemp);

		// Description
		MSG_GetFilterDesc( *m_hFilter, (char **) &lpszTemp);
		m_lpszFilterDesc = lpszTemp;
		widget = GetDlgItem( IDC_EDIT_DESC );
		widget->SetWindowText(lpszTemp);

		// Enabled
		XP_Bool bEnabled;
		MSG_IsFilterEnabled( *m_hFilter, &bEnabled );
		m_iActive = bEnabled ? 1 : 0;
		button = (CButton *) GetDlgItem( IDC_RADIO_OFF );
		button->SetCheck(m_iActive ? 0 : 1 );
		button = (CButton *) GetDlgItem( IDC_RADIO_ON );
		button->SetCheck(m_iActive);

		// Terms
		long iNumTerms;
	        	
		MSG_SearchAttribute attrib;
		MSG_SearchOperator op;
		MSG_SearchValue value;
		MSG_Rule *rule;
		const char *arbitraryHeaders;

		MSG_GetFilterRule( *m_hFilter, &rule );

		MSG_RuleGetNumTerms( rule, &iNumTerms );
		ASSERT(iNumTerms > 0 && iNumTerms < 6);
		

		for ( i = 0; i < (uint16)iNumTerms; i++) {
			if (i > 0) {
				OnMore();
			}
			MSG_RuleGetTerm(rule, i, &attrib, &op, &value, &m_bLogicType, (char **)&arbitraryHeaders);
			SetTerm(i, attrib, op, value, (char*)arbitraryHeaders);
		}

		m_bLogicType = !m_bLogicType;  //we have to switch it since the combo box has these values reversed by index
									   //the backend stores AND = 1  and OR = 0.
		CComboBox *comboAndOr = (CComboBox*) GetDlgItem(IDC_COMBO_AND_OR);
		if (comboAndOr)
		{	
			comboAndOr->SetCurSel(m_bLogicType);
		}

		ChangeLogicText();
		void *value2;

		MSG_RuleGetAction( rule, &type, &value2);
		SetAction( type, value2 );
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CFilterDialog::UpdateColumn1Attributes()
{
	uint16 i, j;
	CWnd *widget = NULL;
	uint16 numItems;
	CComboBox *combo = NULL;

	MSG_FolderInfo *pInbox = NULL;
	MSG_GetFoldersWithFlag (WFE_MSGGetMaster(), MSG_FOLDER_FLAG_INBOX, &pInbox, 1);
	MSG_GetNumAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, &numItems); 
    MSG_SearchMenuItem * HeaderItems = new MSG_SearchMenuItem [numItems+1];
	
	if (!HeaderItems) 
		return;  //something bad happened here!!

	//we are getting the list of Headers and updating controls in column 1 
	MSG_GetAttributesForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**)&pInbox, 1, HeaderItems, &numItems);

	CString strEditCustom;
	strEditCustom.LoadString(IDS_EDIT_CUSTOM);
	strcpy( HeaderItems[numItems].name,  strEditCustom);
	HeaderItems[numItems].attrib = -1;  //identifies a command to launch the Edit headers dialog
	HeaderItems[numItems].isEnabled = FALSE;
	BOOL bUsesCustomHeaders = MSG_ScopeUsesCustomHeaders(WFE_MSGGetMaster(),scopeMailFolder, pInbox, TRUE);


	for (i = 0; i < 5; i++) {

		combo = (CComboBox *) GetDlgItem( RuleMatrix[i][COL_ATTRIB] );
		if (!combo)
		{
			delete HeaderItems; //free the memory since it's now in the combo box
			return;
		}
		combo->ResetContent();
		for (j = 0; j < numItems; j++) {
			combo->AddString(HeaderItems[j].name);
			combo->SetItemData(j, HeaderItems[j].attrib);
		}
		if (j == numItems && bUsesCustomHeaders)
		{   //place the edit text in the last position of the combo box
			combo->AddString(HeaderItems[j].name);
			combo->SetItemData(j, HeaderItems[j].attrib);
		}

		combo->SetCurSel(0);
		UpdateOpList(i);
	}

	delete HeaderItems; //free the memory since it's now in the combo box
}


void CFilterDialog::EditHeaders(int iRow)
{
	//We are being asked to modify custom headers

	CComboBox *combo;	
	int iCurSel;
	MSG_SearchAttribute attrib;
	m_iRowSelected = iRow;
	combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_ATTRIB] );
	iCurSel = combo->GetCurSel();
	attrib = (MSG_SearchAttribute) combo->GetItemData(iCurSel);

	if (attrib == -1)
	{
		MSG_Master *master = WFE_MSGGetMaster();
		//find out if we are the only ones trying to edit headers.
		if (master)
		{
			if (!MSG_AcquireEditHeadersSemaphore(master, this))
			{
				::MessageBox(this->GetSafeHwnd(),
						 szLoadString(IDS_EDIT_HEADER_IN_USE), 
						 szLoadString(IDS_CUSTOM_HEADER_ERROR), 
						 MB_OK|MB_ICONSTOP);
				combo->SetCurSel(0);
				return;
			}
		}
		else
		{
			return;
		}

		m_pCustomHeadersDlg = new CCustomHeadersDlg(this);
		if (m_pCustomHeadersDlg)
		{
			m_pCustomHeadersDlg->ShowWindow(SW_SHOW);
		}
	}
}

LONG CFilterDialog::OnFinishedHeaders(WPARAM wParam, LPARAM lParam )
{
	MSG_Master *master = WFE_MSGGetMaster();
	CComboBox *pCombo = (CComboBox *) GetDlgItem( RuleMatrix[m_iRowSelected][COL_ATTRIB] );
	if (lParam == IDOK )
	{
		UpdateColumn1Attributes();
	}
	else
	{
		pCombo->SetCurSel(0);
	}
	MSG_ReleaseEditHeadersSemaphore(master, this);
	m_pCustomHeadersDlg = NULL;
	return 0;
}

void CFilterDialog::OnNewFolder()
{
	int iCurSel = m_FolderCombo.GetCurSel();
	MSG_FolderInfo *pFolderInfo = (MSG_FolderInfo*)m_FolderCombo.GetItemData(iCurSel);
	if (pFolderInfo)
	{
		CNewFolderDialog(this, m_pPane, pFolderInfo);
	}
	else
	{
		CNewFolderDialog(this,m_pPane, NULL);
	}
	OnUpdateDest();
}


void CFilterDialog::UpdateOpList(int iRow)
{
	CComboBox *combo;
	CNSDateEdit *date;
	CWnd *widget, *widgetPrior;
	CEdit *edit;

	uint16 j, iCurSel;
	MSG_SearchAttribute attrib;

	combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_ATTRIB] );
	iCurSel = combo->GetCurSel();
	attrib = (MSG_SearchAttribute) combo->GetItemData(iCurSel);

	MSG_SearchMenuItem items[16];
	uint16 maxItems = 16;

	combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_OP] );
	combo->ResetContent();

	MSG_FolderInfo *pInbox = NULL;
	MSG_GetFoldersWithFlag (WFE_MSGGetMaster(), MSG_FOLDER_FLAG_INBOX, &pInbox, 1);
	MSG_GetOperatorsForFilterScopes (WFE_MSGGetMaster(), scopeMailFolder, (void**) &pInbox, 1, attrib, items, &maxItems);

	for (j = 0; j < maxItems; j++) {
		combo->AddString(items[j].name);
		combo->SetItemData(j, items[j].attrib);
	}
	combo->SetCurSel(0);

	// Calculate a dialog unit 
	RECT rect = {0, 0, 1, 1};
	::MapDialogRect(this->m_hWnd, &rect);
	int iDialogUnit = rect.right;

	POINT ptUs = {0,0};
	ClientToScreen(&ptUs);

	//
	// Substitute a happy new widget based on the search attribute
	//

	if ( attrib == AttribWas [iRow] ) {
		return;
	}

	AttribWas[iRow] = attrib;

	MSG_SearchValueWidget valueWidget;
	MSG_GetSearchWidgetForAttribute ( attrib, &valueWidget );

	if (( WidgetWas[iRow] == valueWidget ) && ( valueWidget != widgetMenu) ) {
		return;
	}

	WidgetWas[iRow] = valueWidget;

	widget = GetDlgItem( RuleMatrix[iRow][COL_VALUE] );
	widgetPrior = GetDlgItem( RuleMatrix[iRow][COL_OP] );
	
	RECT rectWidget;
	widget->GetWindowRect(&rectWidget);
	::OffsetRect( &rectWidget, -ptUs.x, -ptUs.y );

	DWORD dwStyle = widget->GetStyle() &
					(WS_CHILD|WS_VISIBLE|WS_DISABLED|WS_GROUP|WS_TABSTOP);
	DWORD dwExStyle = 0;
	widget->DestroyWindow();

	switch( valueWidget ) {
	case widgetMenu:
		rectWidget.bottom = rectWidget.top + 60 * iDialogUnit;
		dwStyle |= WS_VSCROLL|CBS_DROPDOWNLIST;
		combo = new CComboBox;
		maxItems = 16;
		combo->Create( dwStyle, rectWidget, this, RuleMatrix[iRow][COL_VALUE] );
		combo->SetFont(GetFont());
		MSG_GetValuesForAttribute( scopeMailFolder, attrib, items, &maxItems );
		for (j = 0; j < maxItems; j++) {
			combo->AddString( items[j].name );
			combo->SetItemData( j, items[j].attrib );
		}
		combo->SetCurSel(0);
		// Fix Tab Order
		combo->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;
	case widgetDate:
#ifdef XP_WIN32
		dwExStyle |= WS_EX_CLIENTEDGE;
		if ( !sysInfo.m_bWin4 )
#endif  
		{
			dwStyle |= WS_BORDER;
		}
		dwStyle |= WS_CLIPCHILDREN;

		date = new CNSDateEdit;
#ifdef XP_WIN32
		date->CreateEx( dwExStyle, _T("STATIC"), "", dwStyle, 
						rectWidget.left, rectWidget.top, 
						rectWidget.right - rectWidget.left, 
						rectWidget.bottom - rectWidget.top,
						this->m_hWnd, (HMENU) RuleMatrix[iRow][COL_VALUE]);
#else
		date->Create( "", dwStyle|SS_SIMPLE, rectWidget, this, RuleMatrix[iRow][COL_VALUE]);
#endif
		date->SetFont( GetFont() );
		date->SetDate( CTime( time( NULL ) ) );
		date->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;
	
	case widgetText:
	default:
		dwStyle |= ES_AUTOHSCROLL;
#ifdef XP_WIN32
		dwExStyle |= WS_EX_CLIENTEDGE;
		::InflateRect(&rectWidget, 
					  GetSystemMetrics(SM_CXEDGE), 
					  GetSystemMetrics(SM_CYEDGE));
		if ( !sysInfo.m_bWin4 )
#endif
		{
			dwStyle |= WS_BORDER;
		}
		edit = new CEdit;
#ifdef _WIN32
		edit->CreateEx( dwExStyle, _T("EDIT"), "", dwStyle, 
						rectWidget.left, rectWidget.top, 
						rectWidget.right - rectWidget.left, 
						rectWidget.bottom - rectWidget.top,
						this->m_hWnd, (HMENU) RuleMatrix[iRow][COL_VALUE]);
#else
		edit->Create( dwStyle, rectWidget, this, RuleMatrix[iRow][COL_VALUE]);
#endif
		edit->SetFont(GetFont());
		edit->SetWindowText("");
		// Fix Tab Order
		edit->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
		break;
	}
}

void CFilterDialog::UpdateDestList()
{
	CComboBox *combo;
	CWnd *widgetPrior;

	uint16 j, iCurSel;
	MSG_RuleActionType action;

	MSG_SearchMenuItem items[16];
	uint16 maxItems = 16;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_ACTION );
	widgetPrior = combo;
	iCurSel = combo->GetCurSel();
	action = (MSG_RuleActionType) combo->GetItemData( iCurSel );

    MSG_SearchValueWidget destWidget;

	MSG_GetFilterWidgetForAction( action, &destWidget );

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_DEST );

	switch (destWidget) {
	case widgetMenu:
		{
			RECT rect = {0, 0, 1, 1};
			::MapDialogRect(this->m_hWnd, &rect);
			int iDialogUnit = rect.right;

			POINT ptUs = {0,0};
			ClientToScreen(&ptUs);

			RECT rectWidget;
			combo->GetWindowRect( &rectWidget );
			::OffsetRect( &rectWidget, -ptUs.x, -ptUs.y );
			rectWidget.bottom = rectWidget.top + 60 * iDialogUnit;
			DWORD dwStyle = combo->GetStyle() &
						(WS_CHILD|WS_VISIBLE|WS_DISABLED|WS_GROUP|WS_TABSTOP);

			switch (action) {
			case acChangePriority:
				if (m_FolderCombo.m_hWnd != NULL) {
					m_FolderCombo.DestroyWindow();

					dwStyle |= WS_VSCROLL|CBS_DROPDOWNLIST;
					combo = new CComboBox;
					combo->Create( dwStyle, rectWidget, this, IDC_COMBO_DEST );
					combo->SetFont(GetFont());
					combo->SetWindowPos( widgetPrior, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);

					combo->ResetContent();
					MSG_GetValuesForAction( action, items, &maxItems );
					for (j = 0; j < maxItems; j++) {
						combo->AddString( items[j].name );
						combo->SetItemData( j, items[j].attrib );
					}
					combo->SetCurSel(0);
				}
				combo->ShowWindow(SW_SHOW);
			break;

			case acMoveToFolder:
				if (m_FolderCombo.m_hWnd == NULL) {
					combo->DestroyWindow();

					dwStyle |= WS_VSCROLL|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED;
					m_FolderCombo.Create( dwStyle, rectWidget, this, IDC_COMBO_DEST );
					m_FolderCombo.SetFont(GetFont());
					m_FolderCombo.SetWindowPos( widgetPrior, 0, 0, 0, 0, 
												SWP_NOSIZE|SWP_NOMOVE);
					m_FolderCombo.PopulateMail(WFE_MSGGetMaster(), FALSE);
					m_FolderCombo.SetCurSel(0);
				}
				m_FolderCombo.ShowWindow(SW_SHOW);
				break;

			default:
				ASSERT(0); // Not handled
			}
		}
		break;

	case widgetNone:
		combo->ShowWindow(SW_HIDE);
		break;

	default:
	case widgetInt:
	case widgetText:
	case widgetDate:
		ASSERT(0); // Not handled
	}
}


int CFilterDialog::ChangeLogicText()
{
	CWnd *widget;

	if (m_iRowIndex <= 4) 
	{
		CString strLogicText;
		for (int Row = 1; Row <= m_iRowIndex; Row++) 
		{
			widget = GetDlgItem(RuleMatrix[Row][3]);
			if (!widget) return 0;
			if (m_bLogicType)
			{   //Display AND logic text
				strLogicText.LoadString(IDS_ORTHE);
				widget->SetWindowText(strLogicText);
			}
			else 
			{   //Display AND logic text
				strLogicText.LoadString(IDS_ANDTHE);
				widget->SetWindowText(strLogicText);
			}
			widget->ShowWindow(SW_SHOW);
		}
	}
	return 1;
}



int CFilterDialog::GetNumTerms()
{
	return m_iRowIndex + 1;
}

static char szResultText[64];

void CFilterDialog::GetTerm( int iRow,
							 MSG_SearchAttribute *attrib,
							 MSG_SearchOperator *op,
							 MSG_SearchValue *value,
							 char *pszArbitraryHeader)
{
	CComboBox *combo;
	CNSDateEdit *date;
	CWnd *widget;
	int iCurSel;

	combo = (CComboBox *) GetDlgItem(RuleMatrix[iRow][COL_ATTRIB]);
	iCurSel = combo->GetCurSel();
	*attrib = (MSG_SearchAttribute) combo->GetItemData(iCurSel);
	CString strHeader;
	combo->GetWindowText(strHeader);
	strcpy(pszArbitraryHeader,strHeader);
	
	combo = (CComboBox *) GetDlgItem(RuleMatrix[iRow][COL_OP]);
	iCurSel = combo->GetCurSel();
	*op = (MSG_SearchOperator) combo->GetItemData(iCurSel);
	
	widget = GetDlgItem(RuleMatrix[iRow][COL_VALUE]);
	widget->GetWindowText(szResultText, sizeof(szResultText));

	value->attribute = *attrib;
	switch (*attrib) {
		case attribDate:
			{
				CTime ctime;
				date = (CNSDateEdit *) GetDlgItem(RuleMatrix[iRow][COL_VALUE]);
				date->GetDate(ctime);
				value->u.date = ctime.GetTime();
			}
			break;
		case attribPriority:
			combo = (CComboBox *) GetDlgItem(RuleMatrix[iRow][COL_VALUE]);
			iCurSel = combo->GetCurSel();
			value->u.priority = (MSG_PRIORITY) combo->GetItemData(iCurSel);
			break;
		case attribMsgStatus:
			combo = (CComboBox *) GetDlgItem(RuleMatrix[iRow][COL_VALUE]);
			iCurSel = combo->GetCurSel();
			value->u.msgStatus = combo->GetItemData(iCurSel);
			break;
		default:
			value->u.string = XP_STRDUP( szResultText );
	}
}

void CFilterDialog::SetTerm( int iRow,
							 MSG_SearchAttribute attrib,
							 MSG_SearchOperator op,
							 MSG_SearchValue value,
							 char *pszArbitraryHeader)
{
	CComboBox *combo;
	CNSDateEdit *date;
	CWnd *widget;
	CString strHeader;
	int i;
	BOOL bFound = FALSE;

	combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_ATTRIB] );
	combo->SetCurSel(0);
	CString strCheckHeader = pszArbitraryHeader;

	if (!strCheckHeader.IsEmpty() ) {
		for ( i = 0; i < (combo->GetCount() -1) ; i++ ) {
			combo->GetLBText(i,strHeader);
			if ( (MSG_SearchAttribute) combo->GetItemData(i) == attrib && !strHeader.Compare(strCheckHeader) ) {
				combo->SetCurSel(i);
	 			bFound=TRUE;
				break;
			}
		}
	}
	else
	{
		for ( i = 0; i < (combo->GetCount() -1) ; i++ ) {
			if ( (MSG_SearchAttribute) combo->GetItemData(i) == attrib ) {
				combo->SetCurSel(i);
	 			bFound=TRUE;
				break;
			}
		}
	}

	//The saved header may not be on the globally stored list of headers.  prompt so they can retain settings
	if (!bFound && (attrib == attribOtherHeader) &&  !strCheckHeader.IsEmpty())
	{
		CString strMessage;
		AfxFormatString1(strMessage,IDS_HEADER_DELETED_WARN,pszArbitraryHeader);
		if (AfxMessageBox(strMessage,MB_OKCANCEL) == IDOK)
		{
			//Update Arbitrary header list
			CString strArbitraryHeaders;
			char *pHeaderList=NULL;
			PREF_CopyCharPref("mailnews.customHeaders",&pHeaderList);
			strArbitraryHeaders = pHeaderList;
			if (!strArbitraryHeaders.IsEmpty())
			{
				strArbitraryHeaders += ": " + (CString)pszArbitraryHeader;
			}
			else if (!strHeader.IsEmpty())
			{
				strArbitraryHeaders = pszArbitraryHeader;
			}

			PREF_SetCharPref("mailnews.customHeaders",strArbitraryHeaders);
			UpdateColumn1Attributes();
			for ( i = 0; i < (combo->GetCount()-1); i++ ) 
			{
				combo->GetLBText(i,strHeader);
				if ( (MSG_SearchAttribute) combo->GetItemData(i) == attrib && !strHeader.Compare(pszArbitraryHeader) ) {
					combo->SetCurSel(i);
					bFound=TRUE;
					break;
				}
			}

		}

	}

	UpdateOpList( iRow );

	combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_OP] );
	combo->SetCurSel(0);
	for ( i = 0; i < combo->GetCount(); i++ ) {
		if ( (MSG_SearchOperator) combo->GetItemData(i) == op) {
			combo->SetCurSel(i);
			break;
		}
	}

	switch ( value.attribute ) {
	case attribDate:
		{
			CTime ctime( value.u.date );
			date = (CNSDateEdit *) GetDlgItem( RuleMatrix[iRow][COL_VALUE] );
			date->SetDate( ctime );
		}
		break;
	case attribPriority:
		combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_VALUE] );
		combo->SetCurSel(0);
		for ( i = 0; i < combo->GetCount(); i++ ) {
			if ( (MSG_PRIORITY) combo->GetItemData(i) == value.u.priority ) {
				combo->SetCurSel(i);
				break;
			}
		}
		break;
	case attribMsgStatus:
		combo = (CComboBox *) GetDlgItem( RuleMatrix[iRow][COL_VALUE] );
		for ( i = 0; i < combo->GetCount(); i++ ) {
			if ( combo->GetItemData(i) == value.u.msgStatus ) {
				combo->SetCurSel(i);
				break;
			}
		}
		break;
	default:
		widget = GetDlgItem( RuleMatrix[iRow][COL_VALUE] );
		widget->SetWindowText( (const char *) value.u.string );
	}
}

void CFilterDialog::GetAction(MSG_RuleActionType *action, void **value)
{
	CComboBox *combo;
	int iCurSel;
	MSG_FolderInfo	*folderID;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_ACTION );
	iCurSel = combo->GetCurSel();
	*action = (MSG_RuleActionType) combo->GetItemData( iCurSel);

	switch (*action) {
	case acMoveToFolder:
		combo = (CComboBox *) GetDlgItem( IDC_COMBO_DEST );
		iCurSel = combo->GetCurSel();
		folderID = (MSG_FolderInfo *) combo->GetItemData( iCurSel);
		* (const char **) value = MSG_GetFolderNameFromID(folderID);
		break;

	case acChangePriority:
		combo = (CComboBox *) GetDlgItem( IDC_COMBO_DEST );
		iCurSel = combo->GetCurSel();
		*value = (void *) combo->GetItemData(iCurSel);
		break;

	default:
		*value = NULL;
	}
}

void CFilterDialog::SetAction(MSG_RuleActionType action, void *value)
{
	CComboBox *combo;
	int i;
	BOOL bSet;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_ACTION );
	combo->SetCurSel(0);
	for ( i = 0; i < combo->GetCount(); i++ ) {
		if ( (MSG_RuleActionType) combo->GetItemData(i) == action) {
			combo->SetCurSel(i);
			break;
		}
	}

	UpdateDestList();

	switch ( action ) {
	case acMoveToFolder:
		combo = (CComboBox *) GetDlgItem( IDC_COMBO_DEST );
		combo->SetCurSel(0);
		for ( i = 0; i < combo->GetCount(); i++ ) {
			char *folderName = (char *) value;
			DWORD dwItemData = combo->GetItemData(i);
			if (!XP_FILENAMECMP(MSG_GetFolderNameFromID((MSG_FolderInfo *) dwItemData), folderName)) {
				combo->SetCurSel(i);
				break;
			}
		}
		break;

	case acChangePriority:
		combo = (CComboBox *) GetDlgItem( IDC_COMBO_DEST );
		combo->SetCurSel(0);
		bSet = FALSE;
		for ( i = 0; i < combo->GetCount(); i++ ) {
			MSG_PRIORITY priority = (MSG_PRIORITY) (int) value;
			if ( ((MSG_PRIORITY) combo->GetItemData(i)) == priority ) {
				combo->SetCurSel(i);
				break;
			}
		}
		break;

	default:
		// Do nothing
		break;
	}
}

BEGIN_MESSAGE_MAP(CFilterDialog, CDialog)
	//{{AFX_MSG_MAP(CFilterDialog)
	ON_BN_CLICKED(IDC_MORE, OnMore)
	ON_BN_CLICKED(IDC_FEWER, OnFewer)
	ON_BN_CLICKED(IDC_NETHELP, OnHelp)
	ON_BN_CLICKED(IDC_BUTTON_NEW_FOLDER, OnNewFolder)
	ON_CBN_DROPDOWN(IDC_COMBO_DEST, OnUpdateDest )
	ON_CBN_SELCHANGE( IDC_COMBO_ACTION, OnAction )
	ON_CBN_SELCHANGE( IDC_COMBO_ATTRIB1, OnAttrib1 )
	ON_CBN_SELCHANGE( IDC_COMBO_ATTRIB2, OnAttrib2 )
	ON_CBN_SELCHANGE( IDC_COMBO_ATTRIB3, OnAttrib3 )
	ON_CBN_SELCHANGE( IDC_COMBO_ATTRIB4, OnAttrib4 )
	ON_CBN_SELCHANGE( IDC_COMBO_ATTRIB5, OnAttrib5 )
	ON_CBN_SELCHANGE( IDC_COMBO_AND_OR,  OnAndOr   )
	ON_MESSAGE(WM_EDIT_CUSTOM_DONE, OnFinishedHeaders )
	//}}AFX_MSG_MAP
#ifndef _WIN32
	ON_MESSAGE(WM_DLGSUBCLASS, OnDlgSubclass)
#endif
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFilterDialog message handlers

static void GrowDialog(CWnd *pWnd, int dx, int dy)
{
	CRect rect;

	pWnd->GetWindowRect(&rect);

	rect.bottom += dy;
	rect.right += dx;

	pWnd->MoveWindow(&rect, TRUE);
	pWnd->Invalidate();
}

static void SlideWindow(CWnd *pWnd, int dx, int dy)
{
	CRect rect;
	CWnd *parent;

	pWnd->GetWindowRect(&rect);
	if (parent = pWnd->GetParent())
		parent->ScreenToClient(&rect);

	rect.top += dy;
	rect.left += dx;
	rect.bottom += dy;
	rect.right += dx;

	pWnd->MoveWindow(&rect, TRUE);
	pWnd->Invalidate();
}

void CFilterDialog::SlideBottomControls(int dy)
{
	CWnd *widget;

	// Move bottom controls	
	widget = GetDlgItem(IDC_STATIC1);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_STATIC2);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_STATIC3);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDCANCEL);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDOK);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_NETHELP);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_EDIT_DESC);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_COMBO_ACTION);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_COMBO_DEST);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_BUTTON_NEW_FOLDER);
	SlideWindow(widget,0,dy);
	widget = GetDlgItem(IDC_RADIO_ON);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_RADIO_OFF);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_FEWER);
	SlideWindow(widget, 0, dy);
	widget = GetDlgItem(IDC_MORE);
	SlideWindow(widget, 0, dy);
	// Grow the dialog
	GrowDialog(this, 0, dy);		
}

void CFilterDialog::OnUpdateDest()
{	
	if (m_FolderCombo.m_hWnd) {
		int idx = m_FolderCombo.GetCurSel();
		DWORD dwData = idx >= 0 ? m_FolderCombo.GetItemData(idx) : 0;
		m_FolderCombo.PopulateMail( WFE_MSGGetMaster(), FALSE );
		m_FolderCombo.SetCurSel(0);
		for ( int i = 0; i < m_FolderCombo.GetCount(); i++ ) {
			DWORD dwItemData = m_FolderCombo.GetItemData(i);
			if (dwItemData == dwData) {
				m_FolderCombo.SetCurSel(i);
				break;
			}
		}
	}
}

void CFilterDialog::OnAction()
{
	UpdateDestList();
}

void CFilterDialog::OnAttrib1()
{
	EditHeaders(0);
	UpdateOpList(0);
}

void CFilterDialog::OnAttrib2()
{
	EditHeaders(1);
	UpdateOpList(1);
}

void CFilterDialog::OnAttrib3()
{
	EditHeaders(2);
	UpdateOpList(2);
}

void CFilterDialog::OnAttrib4()
{
	EditHeaders(3); 
	UpdateOpList(3);
}

void CFilterDialog::OnAttrib5()
{
	EditHeaders(4);
	UpdateOpList(4);
}

void CFilterDialog::OnAndOr()
{
	CComboBox *combo;
	int iCurSel;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_AND_OR );
	if (!combo) return;
	iCurSel = combo->GetCurSel();
	m_bLogicType = (iCurSel == 0 ? 0 : 1);
	ChangeLogicText();
}

void CFilterDialog::OnMore() 
{
	// TODO: Add your control notification handler code here
	CWnd *widget;
	CRect rect, rect2;
	int j, dy;
	CString strLogicText;
	
	// Calculate how much we need to expand for a new rule
	widget = GetDlgItem(RuleMatrix[m_iRowIndex][0]);
	widget->GetWindowRect(&rect);
	m_iRowIndex++;
	for (j = 0; j < COL_COUNT; j++) {
		widget = GetDlgItem(RuleMatrix[m_iRowIndex][j]);
		if (j == 3 && m_bLogicType)
		{
			strLogicText.LoadString(IDS_ORTHE);
			widget->SetWindowText(strLogicText);
		}
		else if ( j==3 && !m_bLogicType)
		{   //change the logic text for this new rule line to AND
			strLogicText.LoadString(IDS_ANDTHE);
			widget->SetWindowText(strLogicText);
		}
		widget->ShowWindow(SW_SHOW);
	}
	widget = GetDlgItem(RuleMatrix[m_iRowIndex][0]);
	widget->GetWindowRect(&rect2);
	dy = rect2.top - rect.top;

	SlideBottomControls(dy);

	if (m_iRowIndex > 3) {
		widget = GetDlgItem(IDC_MORE);
		widget->EnableWindow(FALSE);
	}
	widget = GetDlgItem(IDC_FEWER);
	widget->EnableWindow(TRUE);

	Invalidate();
}

void CFilterDialog::OnFewer() 
{
	// TODO: Add your control notification handler code here
	CWnd *widget;
	CRect rect, rect2;
	int j, dy;

	// Calculate how much we need to expand for a new rule
	widget = GetDlgItem(RuleMatrix[m_iRowIndex][0]);
	widget->GetWindowRect(&rect);
	for (j = 0; j < COL_COUNT; j++) {
		widget = GetDlgItem(RuleMatrix[m_iRowIndex][j]);
		widget->ShowWindow(SW_HIDE);
	}
	m_iRowIndex--;
	widget = GetDlgItem(RuleMatrix[m_iRowIndex][0]);
	widget->GetWindowRect(&rect2);
	dy = rect2.top - rect.top;
	
	SlideBottomControls(dy);

	if (m_iRowIndex < 1) {	
		widget = GetDlgItem(IDC_FEWER);
		widget->EnableWindow(FALSE);	
	}
	widget = GetDlgItem(IDC_MORE);
	widget->EnableWindow(TRUE);

	Invalidate();
}

void CFilterDialog::OnHelp()
{
	NetHelp(HELP_FILTER_RULES);
}

void CFilterDialog::OnOK() 
{
	CDialog::OnOK();

	// TODO: Add extra validation here

	if (*m_hFilter != NULL) {
		MSG_DestroyFilter( *m_hFilter );
	}

	MSG_FilterType filterType = filterInboxRule;

	MSG_FolderInfo *folder = (m_pPane) ? MSG_GetCurFolder(m_pPane) : NULL;
	if (folder && MSG_GetFolderFlags(folder) &  MSG_FOLDER_FLAG_NEWSGROUP)
		filterType = filterNewsRule;

	MSG_CreateFilter(filterType, 
					 (char *) (const char *) m_lpszFilterName, 
					  m_hFilter);
	
	int i;
	MSG_Rule *rule;

	MSG_SetFilterDesc( *m_hFilter, (char *) (const char *) m_lpszFilterDesc );
	MSG_EnableFilter( *m_hFilter, m_iActive ? TRUE : FALSE );

	MSG_GetFilterRule( *m_hFilter, &rule );

	MSG_SearchAttribute attrib;
	MSG_SearchOperator op;
	MSG_SearchValue value;
	char szHeader[67];

	for (i = 0; i < GetNumTerms(); i++) {
//TODO add support for arbitrary headers
		GetTerm(i, &attrib, &op, &value, szHeader);
		MSG_RuleAddTerm( rule, attrib, op, &value, !m_bLogicType, szHeader);
	}

	MSG_RuleActionType action;
	void *value2;

	GetAction( &action, &value2 ),
	MSG_RuleSetAction( rule, action, value2 );
}

#ifndef _WIN32
LRESULT CFilterDialog::OnDlgSubclass(WPARAM wParam, LPARAM lParam)
{
	*(int FAR*) lParam = 0;

	return 0;
}
#endif

