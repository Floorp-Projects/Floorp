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
// srchfrm.cpp : implementation file
//

#include "stdafx.h"
#include "srchfrm.h"
#include "fldrfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "abdefn.h"
#include "addrbook.h"
#include "wfemsg.h"
#include "dirprefs.h"
#include "xp_time.h"
#include "xplocale.h"
#include "template.h"
#include "dateedit.h"
#include "apimsg.h"
#include "nethelp.h"
#include "xp_help.h"
#include "fegui.h"
#include "intl_csi.h"
#include "msg_srch.h"
#include "advopdlg.h"
#include "edhdrdlg.h"
#include "srchobj.h"
#include "shcut.h"
#include "msgcom.h"
#include "mailqf.h"
#include "rdfglobal.h"

#define SEARCH_GROW_HEIGHT 200

#define SCOPE_SELECTED                  0
#define SCOPE_ALL_MAIL                  1
#define SCOPE_SUBSCRIBED_NEWS			2
#define SCOPE_ALL_NEWS                  3

#define DEF_VISIBLE_COLUMNS				4
#define LDAPSEARCH_SOURCETARGET_FORMAT "Netscape LDAP Search source-target"
#define NETSCAPE_SEARCH_FORMAT "Netscape Search"

CSearchFrame            *g_pSearchWindow = NULL;
CLDAPSearchFrame        *g_pLDAPSearchWindow = NULL;

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CSearchView, COutlinerView)
IMPLEMENT_DYNCREATE(CLDAPSearchView, COutlinerView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

// Minor space saver

static _TCHAR szResultText[64];

/////////////////////////////////////////////////////////////////////////////
// CSearchBar

static void SlideWindow( CWnd *pWnd, int dx, int dy )
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
}

static void GrowWindow( CWnd *pWnd, int dx, int dy )
{
	CRect rect;
	CWnd *parent;

	pWnd->GetWindowRect(&rect);
	if (parent = pWnd->GetParent())
		parent->ScreenToClient(&rect);

	rect.bottom += dy;
	rect.right += dx;

	pWnd->MoveWindow(&rect, TRUE);
}

CSearchBar::CSearchBar()
{
	m_iMoreCount = 0;
	m_iHeight = 0;
	m_iWidth = 0;
	m_bLogicType = 0;
}

CSearchBar::~CSearchBar()
{
}

int CSearchBar::GetHeightNeeded()
{
	CRect rect, rect2, rect3;
	CWnd *widget = GetDlgItem(IDC_MORE);
	ASSERT(widget);
	GetWindowRect(&rect);
	widget->GetWindowRect(&rect2);

	CWnd *widget2 = NULL;

	if(m_bLDAP)
	{
		widget2 = GetDlgItem(IDC_ADVANCED_SEARCH);
	}
	else
	{
		widget2 = GetDlgItem(IDC_SEARCHHELP);
	}

	if (widget2)
		widget2->GetWindowRect(&rect3);
	else
		return 0;

	//because of help button
	int nHeight = (rect3.bottom > rect2.bottom) ? rect3.bottom : rect2.bottom;
	return nHeight - rect.top + 8;
}

MSG_ScopeAttribute CSearchBar::DetermineScope( DWORD dwItemData )
{
	MSG_Pane *pPane = NULL;
	MSG_ScopeAttribute scope = scopeMailFolder;

	if ( m_bLDAP ) {
		scope = scopeLdapDirectory;
	} else {
		MSG_FolderLine folderLine;
		if (MSG_GetFolderLineById(WFE_MSGGetMaster(), (MSG_FolderInfo *) dwItemData, &folderLine)) {
			if (folderLine.flags & MSG_FOLDER_FLAG_MAIL) {
				scope = scopeMailFolder;	// Yeah, it's redundant
			} else if (folderLine.flags & (MSG_FOLDER_FLAG_NEWS_HOST|MSG_FOLDER_FLAG_NEWSGROUP)) {
				scope = scopeNewsgroup;
			}
		}
	}
	return scope;
}

void CSearchBar::UpdateAttribList()
{
	CComboBox* combo;
	int iScopeCurSel;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_SCOPE );
	iScopeCurSel = combo->GetCurSel();
	DWORD dwItemData = combo->GetItemData(iScopeCurSel);

	MSG_ScopeAttribute scope = DetermineScope( dwItemData );

	m_searchObj.UpdateAttribList (scope);
}

void CSearchBar::InitializeAttributes (MSG_SearchValueWidget widgetValue, MSG_SearchAttribute attribValue)
{
	m_searchObj.InitializeAttributes (widgetValue, attribValue);
}

void CSearchBar::UpdateOpList()
{
	CComboBox* combo;
	int iScopeCurSel;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_SCOPE );
	iScopeCurSel = combo->GetCurSel();
	DWORD dwItemData = combo->GetItemData(iScopeCurSel);

	MSG_ScopeAttribute scope = DetermineScope( dwItemData );

	m_searchObj.UpdateOpList (scope);
}


void CSearchBar::UpdateOpList(int iRow)
{
	CComboBox* combo;
	int iScopeCurSel;

	combo = (CComboBox *) GetDlgItem( IDC_COMBO_SCOPE );
	iScopeCurSel = combo->GetCurSel();
	DWORD dwItemData = combo->GetItemData(iScopeCurSel);

	MSG_ScopeAttribute scope = DetermineScope( dwItemData );

	m_searchObj.UpdateOpList (iRow, scope);
}

int CSearchBar::More()
{
	int dy = 0;

	dy = m_searchObj.More (&m_iMoreCount, m_bLogicType);

#ifndef _WIN32
	m_sizeFixedLayout.cy = GetHeightNeeded();
#endif

	return dy;
}

int CSearchBar::ChangeLogicText()
{
	m_searchObj.ChangeLogicText (m_iMoreCount, m_bLogicType);

	return 1;
}


int CSearchBar::Fewer()
{
	int dy = 0;

	dy = m_searchObj.Fewer(&m_iMoreCount, m_bLogicType);

#ifndef _WIN32
	m_sizeFixedLayout.cy = GetHeightNeeded();
#endif

	return dy;
}

void CSearchBar::Advanced()
{
}

void CSearchBar::OnAndOr()
{
	m_searchObj.OnAndOr (m_iMoreCount, &m_bLogicType);
}

int CSearchBar::ClearSearch(BOOL bIsLDAPSearch)
{
	int dy = 0, res = 0;

	res = m_searchObj.ClearSearch (&m_iMoreCount, bIsLDAPSearch);

	//We need to tell the frame to shrink
	if ( !m_iMoreCount  && !res)
	{
		CRect rect;
		GetParent()->GetWindowRect(&rect);
		if (rect.Height() > m_iOrigFrameHeight)
			res =  (m_iOrigFrameHeight - rect.Height());  //We need to tell the frame to shrink
	}

	UpdateAttribList();
	UpdateOpList();

	m_searchObj.ReInitializeWidgets();
	
#ifndef _WIN32
	m_sizeFixedLayout.cy = GetHeightNeeded();
#endif

	return res;
}

void CSearchBar::BuildQuery(MSG_Pane* searchPane)
{
	m_searchObj.BuildQuery (searchPane, m_iMoreCount, m_bLogicType);
}

/////////////////////////////////////////////////////////////////////////////
// CSearchBar overloaded methods

BOOL CSearchBar::Create(CWnd* pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID)
{
	BOOL res = CDialogBar::Create(pParentWnd, nIDTemplate, nStyle, nID);

	CRect rect, rect2;
	int dy = 0;

	dy = m_searchObj.New (this);
                
	m_iHeight = m_sizeDefault.cy - dy;
#ifndef _WIN32
	m_sizeFixedLayout.cy = m_iHeight;
#endif
	m_iWidth = m_sizeDefault.cx;

	return res;
}

CSize CSearchBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)    
{
	CSize size;
	size.cx = (bStretch && bHorz ? 32767 : m_sizeDefault.cx );
	size.cy = GetHeightNeeded ( );
	return size;    
}

/////////////////////////////////////////////////////////////////////////////
// CSearchBar Message handlers

BEGIN_MESSAGE_MAP(CSearchBar, CDialogBar)
	//{{AFX_MSG_MAP(CSearchFrame)
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
#ifndef _WIN32
	ON_MESSAGE(WM_DLGSUBCLASS, OnDlgSubclass)
#endif
END_MESSAGE_MAP()

int CSearchBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
    int retval =  CDialogBar::OnCreate( lpCreateStruct);

	CRect rect;
	GetWindowRect(&rect);
	m_sizeDefault = rect.Size();

	return retval;
}

void CSearchBar::OnSize( UINT nType, int cx, int cy )
{
	CDialogBar::OnSize( nType, cx, cy );
	if ( cx && m_iWidth && ( cx != m_iWidth ) ) {
		CWnd *widget;
		int dx = cx - m_iWidth;
		
		widget = GetDlgItem(IDC_FIND);
		SlideWindow(widget, dx, 0);
		widget = GetDlgItem(IDC_SAVE);
		if (widget)
			SlideWindow(widget, dx, 0);
		widget = GetDlgItem(IDC_SEARCHHELP);
		SlideWindow(widget, dx, 0);

		widget = GetDlgItem(IDC_ADVANCED_SEARCH);
		SlideWindow(widget, dx, 0);


		m_searchObj.OnSize (nType, cx, cy, dx);
		m_iWidth = cx;
	}
}

#ifndef _WIN32
LRESULT CSearchBar::OnDlgSubclass(WPARAM wParam, LPARAM lParam)
{
	*(int FAR*) lParam = 0;

	return 0;
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewsMsgList

class CSearchResultsList: public IMsgList {

	CSearchFrame *m_pControllingFrame;
	unsigned long m_ulRefCount;

public:
// IUnknown Interface
	STDMETHODIMP                    QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)    AddRef(void);
	STDMETHODIMP_(ULONG)    Release(void);

// IMsgList Interface
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);

	virtual void GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus);
	virtual void SelectItem( MSG_Pane* pane, int item );
	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo);
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo);

	CSearchResultsList( CSearchFrame *pControllingFrame ) {
		m_ulRefCount = 0;
		m_pControllingFrame = pControllingFrame;
	}
};

STDMETHODIMP CSearchResultsList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
		*ppv = (LPMSGLIST) this;

	if (*ppv != NULL) {
		((LPUNKNOWN) *ppv)->AddRef();
		return NOERROR;
	}
	    
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CSearchResultsList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CSearchResultsList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;    
	return ulRef;           
}

void CSearchResultsList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pControllingFrame) {
		m_pControllingFrame->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CSearchResultsList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pControllingFrame) {
		m_pControllingFrame->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CSearchResultsList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CSearchResultsList::SelectItem( MSG_Pane* pane, int item )
{
}

void CSearchResultsList::MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo)
{



	MSG_DragEffect  effect = MSG_DragMessagesIntoFolderStatus(pane,
									indices,
									count,
									folderInfo,
									MSG_Require_Move);
	if (effect != MSG_Drag_Not_Allowed)
		MSG_MoveMessagesIntoFolder(pane, indices, count, folderInfo);
	
}

void CSearchResultsList::CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) 
{
	MSG_DragEffect  effect = MSG_DragMessagesIntoFolderStatus(pane,
									indices,
									count,
									folderInfo,
									MSG_Require_Copy);
	if (effect != MSG_Drag_Not_Allowed)
		MSG_CopyMessagesIntoFolder(pane, indices, count, folderInfo);
}

/////////////////////////////////////////////////////////////////////////////
// CSearchFrame

CSearchFrame::CSearchFrame()
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(GetContext());
	m_cxType = SearchCX;

	GetContext()->type = MWContextSearch;
	GetContext()->fancyFTP = TRUE;
	GetContext()->fancyNews = TRUE;
	GetContext()->intrupt = FALSE;
	GetContext()->reSize = FALSE;
	INTL_SetCSIWinCSID(csi, CIntlWin::GetSystemLocaleCsid());

	m_pMaster = WFE_MSGGetMaster();

	m_iHeight = 0;
	m_iWidth = 0;
	m_bResultsShowing = FALSE;
	m_bSearching = FALSE;
	m_bIsLDAPSearch = FALSE;
	m_bDragCopying = FALSE;

	m_listSearch = XP_ListNew();
	m_listResult = NULL;
	m_pOutliner = NULL;
	m_pAdvancedOptionsDlg = NULL;
	m_pCustomHeadersDlg = NULL;

	CSearchResultsList *pInstance = new CSearchResultsList (this);
	pInstance->QueryInterface (IID_IMsgList, (LPVOID *) &m_pIMsgList);

	m_pSearchPane = MSG_CreateSearchPane (GetContext(), WFE_MSGGetMaster());
	MSG_SetFEData (m_pSearchPane, pInstance);
}

CSearchFrame::~CSearchFrame()
{
	MSG_SearchFree (m_pSearchPane);
	MSG_DestroyPane (m_pSearchPane);
	m_pIMsgList->Release();
}

#ifndef _WIN32

CWnd* CSearchFrame::CreateView(CCreateContext* pContext, UINT nID)
{
	ASSERT(m_hWnd != NULL);
	ASSERT(::IsWindow(m_hWnd));
	ASSERT(pContext != NULL);
	ASSERT(pContext->m_pNewViewClass != NULL);

	// Note: can be a CWnd with PostNcDestroy self cleanup
	CWnd* pView = (CWnd*)pContext->m_pNewViewClass->CreateObject();
	if (pView == NULL)
	{
		TRACE1("Warning: Dynamic create of view type %hs failed.\n",
			pContext->m_pNewViewClass->m_lpszClassName);
		return NULL;
	}
	ASSERT(pView->IsKindOf( RUNTIME_CLASS( CWnd ) ));

	// views are always created with a border!
	if (!pView->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
		CRect(0,0,0,0), this, nID, pContext))
	{
		TRACE0("Warning: could not create view for frame.\n");
		return NULL;        // can't continue without a view
	}

	return pView;
}

#endif

void CSearchFrame::AdjustHeight(int dy)
{
	CRect rect;
	GetWindowRect(&rect);

	CSize size = rect.Size();
	size.cy += dy; //plus fudge for group box bottom margin
	m_iHeight = size.cy;

	SetWindowPos( NULL, 0, 0, size.cx, size.cy,
				  SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
}

void CSearchFrame::ShowResults( BOOL bShow )
{
	if ( bShow != m_bResultsShowing ) {
		if (bShow) {
			CRect rect;
			GetWindowRect(&rect);
			CSize size = rect.Size();
			m_iHeight += SEARCH_GROW_HEIGHT;
			SetWindowPos( NULL, 0, 0, size.cx, m_iHeight,
						  SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);

			m_barStatus.ShowWindow(SW_SHOW);		

			RecalcLayout();

		} else {

			CRect rect;
			GetWindowRect(&rect);
			CSize size = rect.Size();
			m_iHeight -= SEARCH_GROW_HEIGHT;
			SetWindowPos( NULL, 0, 0, size.cx, m_iHeight,
						  SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);

			RecalcLayout();

			m_pOutliner->SetTotalLines(0);
			m_pOutliner->SelectItem(0);
		}
		m_bResultsShowing = bShow;
	}
}

void CSearchFrame::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
							 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
							 int32 num)
{
	if ( pane == (MSG_Pane*) m_pSearchPane ) 
	{
		if ( m_pOutliner ) 
		{
			m_pOutliner->MysticStuffStarting( asynchronous, notify,where, num );
		}
	}
}

void CSearchFrame::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
							 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
							 int32 num)
{
	if ( pane == (MSG_Pane*) m_pSearchPane ) 
	{
		if ( m_pOutliner ) 
		{
			m_pOutliner->MysticStuffFinishing( asynchronous, notify, where, num );
		}
	}
}

void CSearchFrame::Progress(MWContext *pContext, const char *pMessage)
{
	m_barStatus.SetPaneText( m_barStatus.CommandToIndex( ID_SEPARATOR), pMessage );
}

void CSearchFrame::SetProgressBarPercent(MWContext *pContext, int32 lPercent)
{
	m_barStatus.SetPercentDone(lPercent);
}

void CSearchFrame::AllConnectionsComplete( MWContext *pContext )
{
	CStubsCX::AllConnectionsComplete( pContext );

	// If we were performing a search as view operation, we want to turn the progress bar cylon 
	// off when the connections are complete because this implies that the search as view op is done!
	if (!m_bSearching) 
		SetProgressBarPercent(pContext, 0); 

	m_bSearching = FALSE;

	int total = m_pOutliner->GetTotalLines();
	CString csStatus;
	if ( total > 1) {
		csStatus.Format( szLoadString(IDS_SEARCHHITS), total );
	} else if ( total > 0 ) {
		csStatus.LoadString( IDS_SEARCHONEHIT );
	} else {
		csStatus.LoadString( IDS_SEARCHNOHITS );
	}
	m_barStatus.SetPaneText( m_barStatus.CommandToIndex( ID_SEPARATOR), csStatus );
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
}

void CSearchFrame::UpdateScopes( CMailNewsFrame *pFrame )
{
	m_barSearch.m_wndScopes.Populate(WFE_MSGGetMaster(), NULL);
	
	MSG_FolderInfo *folderInfo = pFrame ? pFrame->GetCurFolder() : NULL;

	m_barSearch.m_wndScopes.SetCurSel(0);
	if (folderInfo) {
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( WFE_MSGGetMaster(), folderInfo, &folderLine );
		if (folderLine.flags & MSG_FOLDER_FLAG_CATEGORY) {
			folderInfo = MSG_GetCategoryContainerForCategory(folderInfo);
		}
		for ( int i = 0; i < m_barSearch.m_wndScopes.GetCount(); i++ ) {
			DWORD dwItemData = m_barSearch.m_wndScopes.GetItemData(i);
			if (dwItemData == (DWORD) folderInfo) {
				m_barSearch.m_wndScopes.SetCurSel(i);
				break;
			}
		}
	}
	m_barSearch.UpdateAttribList();
	m_barSearch.UpdateOpList();
}

/////////////////////////////////////////////////////////////////////////////
// CSearchFrame Overloaded methods

void CSearchFrame::Create()
{
	DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN;
	CString strFullString, strTitle;
	strFullString.LoadString( IDR_SEARCHFRAME );
	AfxExtractSubString( strTitle, strFullString, 0 );

	LPCTSTR lpszClass = GetIconWndClass( dwDefaultStyle, IDR_SEARCHFRAME );
	LPCTSTR lpszTitle = strTitle;
	CFrameWnd::Create(lpszClass, lpszTitle, dwDefaultStyle);

	ActivateFrame();
}

BOOL CSearchFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.hwndParent = NULL;
	return CFrameWnd::PreCreateWindow(cs);
}

BOOL CSearchFrame::OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext )
{
	CWnd *pWnd;
	CCreateContext Context;
	Context.m_pNewViewClass = RUNTIME_CLASS(CSearchView);
	
	if ( pWnd = CreateView(&Context) ) {
		COutlinerView *pView = (COutlinerView *) pWnd;
		pView->CreateColumns ( );
		m_pOutliner = (CSearchOutliner *) pView->m_pOutlinerParent->m_pOutliner;
		m_pOutliner->SetContext( GetContext() );
		m_pOutliner->SetPane (m_pSearchPane);
	} else {
		return FALSE;
	}


	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CSearchFrame message handlers

BEGIN_MESSAGE_MAP(CSearchFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_MORE, OnMore)
	ON_UPDATE_COMMAND_UI( IDC_MORE, OnUpdateMore )
	ON_BN_CLICKED(IDC_FEWER, OnFewer)
	ON_UPDATE_COMMAND_UI( IDC_FEWER, OnUpdateFewer )
	ON_BN_CLICKED(IDC_FIND, OnFind)
	ON_UPDATE_COMMAND_UI( IDC_FIND, OnUpdateFind )
	ON_BN_CLICKED(IDC_TO, OnTo)
	ON_UPDATE_COMMAND_UI( IDC_TO, OnUpdateTo )
	ON_BN_CLICKED(IDC_BUTTON_FILE_MESSAGE, OnFileButton)
	ON_UPDATE_COMMAND_UI( IDC_BUTTON_FILE_MESSAGE, OnUpdateFileButton )

	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_UPDATE_COMMAND_UI(IDC_NEW, OnUpdateQuery)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_UPDATE_COMMAND_UI( IDC_SAVE, OnUpdateSave )
	ON_BN_CLICKED(IDC_SEARCHHELP, OnHelp)
	ON_UPDATE_COMMAND_UI( IDC_SEARCHHELP, OnUpdateHelp )
	ON_BN_CLICKED(IDC_ADVANCED_SEARCH, OnAdvanced)
	ON_UPDATE_COMMAND_UI( IDC_ADVANCED_SEARCH, OnUpdateAdvanced )
	ON_CBN_SELCHANGE(IDC_COMBO_SCOPE, OnScope)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB1, OnAttrib1)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB2, OnAttrib2)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB3, OnAttrib3)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB4, OnAttrib4)
	ON_CBN_SELCHANGE(IDC_COMBO_ATTRIB5, OnAttrib5)
	ON_CBN_SELCHANGE(IDC_COMBO_AND_OR, OnAndOr)
	ON_UPDATE_COMMAND_UI(IDC_COMBO_AND_OR, OnUpdateAndOr)
	ON_MESSAGE(WM_ADVANCED_OPTIONS_DONE, OnFinishedAdvanced)
	ON_MESSAGE(WM_EDIT_CUSTOM_DONE, OnFinishedHeaders)

	ON_COMMAND(ID_EDIT_DELETEMESSAGE, OnDeleteMessage)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETEMESSAGE, OnUpdateDeleteMessage)

	ON_COMMAND(ID_FILE_OPENMESSAGE, OnOpenMessage)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENMESSAGE, OnUpdateOpenMessage)

#ifdef ON_COMMAND_RANGE
	ON_COMMAND_RANGE(FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnFileMessage )
#endif

#ifdef ON_UPDATE_COMMAND_UI_RANGE
	ON_UPDATE_COMMAND_UI_RANGE( IDC_COMBO_ATTRIB1, IDC_EDIT_VALUE5, OnUpdateQuery )
	ON_UPDATE_COMMAND_UI_RANGE( FIRST_MOVE_MENU_ID, LAST_MOVE_MENU_ID, OnUpdateFile )

#endif
	ON_UPDATE_COMMAND_UI( ID_MESSAGE_FILE, OnUpdateFile )
	ON_UPDATE_COMMAND_UI( IDC_COMBO_SCOPE, OnUpdateQuery )

END_MESSAGE_MAP()

#ifndef ON_COMMAND_RANGE

BOOL CSearchFrame::OnCommand( WPARAM wParam, LPARAM lParam )
{
	UINT nID = wParam;

	if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
		OnFileMessage( nID );
		return TRUE;
	}
	return CSearchFrame::OnCommand( wParam, lParam );
}

#endif


#ifndef ON_UPDATE_COMMAND_UI_RANGE

BOOL CSearchFrame::OnCmdMsg( UINT nID, int nCode, void* pExtra, 
							 AFX_CMDHANDLERINFO* pHandlerInfo ) 
{
	if ((nID >= IDC_COMBO_ATTRIB1) && (nID <= IDC_EDIT_VALUE5) && 
		( nCode == CN_UPDATE_COMMAND_UI) ) {
		OnUpdateQuery( (CCmdUI *) pExtra );
		return TRUE;
	}
	else if (nCode == CN_UPDATE_COMMAND_UI) 
	{
		CCmdUI* pCmdUI = (CCmdUI*)pExtra;
		if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) {
			OnUpdateFile( pCmdUI );
			return TRUE;
		}
		if ( nID >= FIRST_COPY_MENU_ID && nID <= LAST_COPY_MENU_ID ) {
			OnUpdateFile( pCmdUI );
			return TRUE;
		}
	}
	return CFrameWnd::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );
}

#endif


void CSearchFrame::OnFileButton()
{
	int nTotalLines = m_pOutliner->GetTotalLines();
	MSG_ViewIndex *indices;
	int iSel;
	m_pOutliner->GetSelection(indices, iSel);
	HMENU hFileMenu = CreatePopupMenu();
	if (!hFileMenu)
		return;  //Bail!!!

	if ( iSel < nTotalLines ) 
	{
		UINT nID = FIRST_MOVE_MENU_ID;
		CMailNewsFrame::UpdateMenu( NULL, hFileMenu, nID );
    }


	CRect rect;
	CWnd *pWidget = (CWnd*) m_barAction.GetDlgItem(IDC_BUTTON_FILE_MESSAGE);
	if (pWidget)
	{   //convert this bad boy to Screen units
		pWidget->GetWindowRect(&rect);
		::MapDialogRect(pWidget->GetSafeHwnd(), &rect);
		pWidget->EnableWindow(FALSE);
	}
	else
	{
		return;//Bail!!
	}

    //	Track the popup now.		
   	DWORD dwError = ::TrackPopupMenu( hFileMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, rect.left, rect.bottom, 0,
					  GetSafeHwnd(), NULL);

	pWidget->EnableWindow(TRUE);


    //  Cleanup handled in CMailNewsFrame
    //VERIFY(::DestroyMenu( hFileMenu ));
}


void CSearchFrame::OnUpdateFileButton(CCmdUI *pCmdUI)
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);
	int nLines = m_pOutliner->GetTotalLines();

	pCmdUI->Enable( ((nLines > 0) && count));
}

void CSearchFrame::OnFileMessage(UINT nID)
{
	if ( m_pSearchPane ) 
	{
		MSG_ViewIndex *indices;
		int count;
		m_pOutliner->GetSelection(indices, count);
		MSG_FolderInfo *folderInfo = FolderInfoFromMenuID( nID );

		ASSERT(folderInfo);
		if (folderInfo) 
		{
			// We want to make file behave for newsgroups
			if ( MSG_GetFolderFlags(folderInfo) & MSG_FOLDER_FLAG_NEWSGROUP ) 
			{
				MSG_CopyMessagesIntoFolder( m_pSearchPane, indices, count, folderInfo);
			} 
			else 
			{
				MSG_MoveMessagesIntoFolder( m_pSearchPane, indices, count, folderInfo);
			}
			ModalStatusBegin( MODAL_DELAY );
		}
	}

}

void CSearchFrame::OnOpenMessage()
{
	BOOL bReuse = g_MsgPrefs.m_bMessageReuse;
	if (GetKeyState(VK_MENU) & 0x8000)
		bReuse = !bReuse;

	MSG_ViewIndex *indices;
	int i, count;
	
	m_pOutliner->GetSelection(indices, count);

	for ( i = 0; i < count; i++ ) {
		MSG_ResultElement *elem = NULL;
		MSG_GetResultElement(m_pSearchPane, indices[i], &elem);

		ASSERT(elem);

		if ( !elem )
			continue;

		MWContextType cxType = MSG_GetResultElementType( elem );

		if ( cxType == MWContextMail || cxType == MWContextMailMsg || 
			 cxType == MWContextNews || cxType == MWContextNewsMsg ) {
			CMessageFrame *frame = CMessageFrame::Open ();
			MSG_OpenResultElement (elem, frame->GetPane());
		} else if ( cxType == MWContextBrowser ) {
			MWContext *pContext = NULL;
			VERIFY(pContext = CFE_CreateNewDocWindow( NULL, NULL ));
			if (pContext) {
				MSG_OpenResultElement( elem, (MSG_Pane *) pContext );
			}
		} else { 
			ASSERT(0); // What on earth are you passing me?
		}
	}
}

void CSearchFrame::OnUpdateOpenMessage(CCmdUI *pCmdUI )
{
	pCmdUI->Enable(TRUE);
}


MSG_FolderInfo *CSearchFrame::FolderInfoFromMenuID( MSG_FolderInfo *mailRoot, 
													  UINT &nBase, UINT nID )
{
	int i, iCount;
	MSG_FolderInfo **folderInfos;
	MSG_FolderInfo *res = NULL;
	MSG_FolderLine folderLine;

	if (mailRoot == NULL ) {
		// Loop through top level folderInfos, looking for mail trees.

		iCount = MSG_GetFolderChildren(m_pMaster, NULL, NULL, NULL);
		folderInfos = new MSG_FolderInfo*[iCount];
		if (folderInfos) {
			MSG_GetFolderChildren(m_pMaster, NULL, folderInfos, iCount);

			for (i = 0; i < iCount && !res; i++) {
				if (MSG_GetFolderLineById(m_pMaster, folderInfos[i], &folderLine)) {
					if (folderLine.flags & MSG_FOLDER_FLAG_MAIL) {
						res = FolderInfoFromMenuID( folderInfos[i], nBase, nID);
					}
				}
			}
			delete [] folderInfos;
		}
		return res;
	}

	MSG_GetFolderLineById( m_pMaster, mailRoot, &folderLine );
	if (folderLine.level > 1) { // We've a subfolder
		if ( nID == nBase ) {
			return mailRoot;
		}
		nBase++;
	}

	iCount = MSG_GetFolderChildren( m_pMaster, mailRoot, NULL, NULL );

	folderInfos = new MSG_FolderInfo*[iCount];
	ASSERT( folderInfos );
	if (folderInfos) {
		MSG_GetFolderChildren( m_pMaster, mailRoot, folderInfos, iCount );

		for ( i = 0; i < iCount && !res; i++ ) {
			if ( MSG_GetFolderLineById( m_pMaster, folderInfos[ i ], &folderLine ) ) {
				if ( folderLine.numChildren > 0 ) {
					res = FolderInfoFromMenuID( folderInfos[ i ], nBase, nID );
				} else {
					if ( nID == nBase ) {
						res = folderInfos[ i ];
					} else {
						nBase++;
					}
				}
			}
		}
		delete [] folderInfos;
	}

	return res;
}

MSG_FolderInfo *CSearchFrame::FolderInfoFromMenuID( UINT nID )
{
	UINT nBase = 0;
	if ( nID >= FIRST_MOVE_MENU_ID && nID <= LAST_MOVE_MENU_ID ) 
	{
		nBase = FIRST_MOVE_MENU_ID;
	}
	else 
	{
		ASSERT(0);
		return NULL;
	}
	return FolderInfoFromMenuID( NULL, nBase, nID );
}



void CSearchFrame::OnUpdateFile( CCmdUI *pCmdUI )
{
	MSG_ViewIndex *indices = NULL;
	int count;
	MSG_ResultElement *elem = NULL;

	m_pOutliner->GetSelection(indices, count);
	MSG_GetResultElement(m_pSearchPane, indices[0], &elem);

	ASSERT(elem);

	if ( !elem )
		return;

	MSG_SearchValue *value;
	MSG_GetResultAttribute(elem, attribMessageKey, &value);

	BOOL bEnable = value->u.key != MSG_MESSAGEKEYNONE;
	if (pCmdUI->m_pSubMenu)
	{
	    pCmdUI->m_pMenu->EnableMenuItem(pCmdUI->m_nIndex,
										MF_BYPOSITION |(bEnable ? MF_ENABLED : MF_GRAYED));
	} 
	else
	{
		pCmdUI->Enable( bEnable );
	}
}

BOOL CSearchFrame::PreTranslateMessage( MSG* pMsg )
{
	if ( (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000))
		return CFrameWnd::PreTranslateMessage(pMsg);

	if (pMsg->message == WM_KEYDOWN && (int) pMsg->wParam == VK_ESCAPE)
		PostMessage(WM_CLOSE);

	if (pMsg->message == WM_KEYDOWN && (int) pMsg->wParam == VK_TAB) {
		HWND hwndNext = NULL;
		HWND hwndFocus = ::GetFocus();

		HWND hwndSearchFirst = ::GetNextDlgTabItem( m_barSearch.m_hWnd, NULL, FALSE );
		HWND hwndActionFirst = ::GetNextDlgTabItem( m_barAction.m_hWnd, NULL, FALSE );

		HWND hwndSearchLast = ::GetNextDlgTabItem( m_barSearch.m_hWnd, hwndSearchFirst, TRUE );
		HWND hwndActionLast = ::GetNextDlgTabItem( m_barAction.m_hWnd, hwndActionFirst, TRUE );

		if ( GetKeyState(VK_SHIFT) & 0x8000 ) {

			// Tab backward

			if ( hwndFocus == hwndSearchFirst ) {
				// Handle tabbing into action bar
				if ( m_bResultsShowing ) {
					hwndNext = hwndActionLast;
					if ( !hwndNext || GetWindowLong( hwndNext, GWL_STYLE) & WS_DISABLED ) {
						// Nothing active in the action bar
						// we can head for the outliner instead
						hwndNext = m_pOutliner->m_hWnd;
					}
				}
			} else if (hwndFocus == m_pOutliner->m_hWnd) {
				// Handle tabbing out of outliner
				hwndNext = hwndSearchLast;
			} else if ( hwndFocus == hwndActionFirst ) {
				// Handle tabbing into the outliner
				hwndNext = m_pOutliner->m_hWnd;
			}

		} else {

			// Tab forward

			if (hwndFocus == m_pOutliner->m_hWnd) {
				// Handle tabbing out of outliner
				hwndNext = hwndActionFirst;
				if ( !hwndNext || GetWindowLong( hwndNext, GWL_STYLE) & WS_DISABLED ) {
					// nothing active in action bar
					hwndNext = hwndSearchFirst;
				}
			} else if ( hwndFocus == hwndSearchLast ) {
				// Handle tabbing into outliner
				if ( m_bResultsShowing ) {
					// we can head for the outliner instead
					hwndNext = m_pOutliner->m_hWnd;
				}
			} else if (hwndFocus == hwndActionLast) {
				// Handle tabbing out of the action bar
				hwndNext = hwndSearchFirst;
			}

		}
		if ( hwndNext ) {
			::SetFocus( hwndNext );
			return TRUE;
		}
	}

	return CFrameWnd::PreTranslateMessage(pMsg);
}

int CSearchFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	UINT aIDArray[] = { IDS_TRANSFER_STATUS, ID_SEPARATOR };
	int res = CFrameWnd::OnCreate(lpCreateStruct);

	SetWindowText(szLoadString(IDS_SEARCHMESSAGES));

	m_barSearch.m_bLDAP = FALSE;

	m_helpString = HELP_SEARCH_MAILNEWS;

#ifdef _WIN32
	m_barSearch.Create( this, IDD_SEARCH, WS_CHILD|CBRS_ALIGN_TOP, 1 );
#else
	m_barSearch.Create( this, IDD_SEARCH, WS_CHILD|CBRS_TOP, 1 );
#endif
	m_barStatus.Create( this, FALSE, FALSE );
	m_barAction.Create( this, IDD_MSGSRCHACTION, WS_CHILD|CBRS_BOTTOM, 2);

	RecalcLayout( );
	
	m_barSearch.InitializeAttributes (widgetText, attribSender);

	m_barSearch.m_wndScopes.SubclassDlgItem(IDC_COMBO_SCOPE, &m_barSearch);
	UpdateScopes( NULL );

	// Initially size window to only dialog + title bar.

	CRect rect, rect2;
	int BorderX = GetSystemMetrics(SM_CXFRAME);
	int BorderY = GetSystemMetrics(SM_CYFRAME);

	GetWindowRect(&rect);
	m_barSearch.GetWindowRect(&rect2);
	CSize size = m_barSearch.CalcFixedLayout(FALSE, FALSE);
	// Figure height of title bar + bottom border
	size.cy += rect2.top - rect.top + BorderY;
	size.cx += BorderX * 2;
	SetWindowPos( NULL, 0, 0, size.cx, size.cy,
				  SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);

	m_iHeight = size.cy;
	m_iWidth = size.cx;

	m_barSearch.m_iOrigFrameHeight = m_iOrigFrameHeight = m_iHeight;
	m_barSearch.m_searchObj.SetOrigFrameHeight(m_barSearch.m_iOrigFrameHeight);

	OnNew();
	

#ifndef _WIN32
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
#endif

	return res;
}

void CSearchFrame::OnSize( UINT nType, int cx, int cy )
{
	CFrameWnd::OnSize( nType, cx, cy );
}

void CSearchFrame::OnClose()
{
	CFrameWnd::OnClose();
	g_pSearchWindow = NULL;
}

void CSearchFrame::OnDestroy()
{
	CFrameWnd::OnDestroy();

	if(!IsDestroyed()) {
		DestroyContext();
	}
}

void CSearchFrame::OnUpdateDeleteMessage(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void CSearchFrame::OnDeleteMessage()
{
	ModalStatusBegin( MODAL_DELAY );

	MSG_ViewIndex *indices = NULL;
	int count = 0;
	m_pOutliner->GetSelection(indices, count);

	MSG_Command(m_pSearchPane, MSG_DeleteMessage, indices, count);

	ModalStatusEnd();
}

void CSearchFrame::ModalStatusBegin( int iModalDelay )
{
	if ( iModalDelay > -1 ) {
	}
}

void CSearchFrame::ModalStatusEnd()
{

}


LONG CSearchFrame::OnFinishedHeaders(WPARAM wParam, LPARAM lParam )
{
	MSG_Master *master = WFE_MSGGetMaster();
	CComboBox *pCombo = (CComboBox *) m_barSearch.m_searchObj.GetColumnOneAttributeWidget(m_iRowSelected);
	if (lParam == IDOK )
	{
		m_barSearch.m_searchObj.UpdateColumn1Attributes();
	}
	else
	{
		pCombo->SetCurSel(0);
	}
	MSG_ReleaseEditHeadersSemaphore(master, this);
	m_pCustomHeadersDlg = NULL;
	return 0;
}


LONG CSearchFrame::OnFinishedAdvanced(WPARAM wParam, LPARAM lParam )
{
	m_barSearch.UpdateAttribList();
	m_barSearch.UpdateOpList();
	m_barSearch.m_searchObj.ReInitializeWidgets();	
	m_pAdvancedOptionsDlg = NULL;
	return 0;
}

void CSearchFrame::OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI )
{
	CFrameWnd::OnGetMinMaxInfo( lpMMI );

	if (m_iHeight) {
		if (!m_bResultsShowing) {
			lpMMI->ptMaxSize.y = m_iHeight;
			lpMMI->ptMaxTrackSize.y = m_iHeight;
		}
		lpMMI->ptMinTrackSize.y = m_iHeight;
	}
	if (m_iWidth) {
		lpMMI->ptMinTrackSize.x = m_iWidth;
	}
}

void CSearchFrame::OnAndOr()
{
	m_barSearch.OnAndOr();
}

void CSearchFrame::OnAdvanced()
{
	m_pAdvancedOptionsDlg = new CAdvSearchOptionsDlg(this);
	if(m_pAdvancedOptionsDlg)
		m_pAdvancedOptionsDlg->ShowWindow(SW_SHOW);

}

void CSearchFrame::OnUpdateAdvanced( CCmdUI *pCmdUI)
{
	pCmdUI->Enable( !m_bSearching );
}


void CSearchFrame::OnMore()
{
	AdjustHeight(m_barSearch.More());
	RecalcLayout( );
#ifndef _WIN32
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
#endif
}

void CSearchFrame::OnUpdateMore( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( (m_barSearch.m_iMoreCount < 4) && !m_bSearching );
}

void CSearchFrame::OnFewer()
{
	AdjustHeight(m_barSearch.Fewer());
	RecalcLayout( );
#ifndef _WIN32
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
#endif
}

void CSearchFrame::OnUpdateFewer( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( (m_barSearch.m_iMoreCount > 0) && !m_bSearching );
}

void CSearchFrame::OnNew()
{
	if (m_bSearching) {
		XP_InterruptContext( GetContext() );
		m_bSearching = FALSE;
	}
	MSG_SearchFree(m_pSearchPane);    /* free memory in context   */

	ShowResults( FALSE );

	m_pOutliner->SelectItem(0);
	int dy = m_barSearch.ClearSearch(m_bIsLDAPSearch);


	AdjustHeight(dy);

#ifndef _WIN32
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
#endif
}

void CSearchFrame::OnSave()
{
	char *name = FE_PromptWithCaption(GetContext(), 
									  szLoadString(IDS_VIRTUALNEWSGROUP),
									  szLoadString(IDS_VIRTUALNEWSGROUPDESC),
									  NULL);
	MSG_SaveProfile(m_pSearchPane, name);
}

void CSearchFrame::OnUpdateSave( CCmdUI *pCmdUI )
{
	XP_Bool fEnable = FALSE;
	MSG_SaveProfileStatus (m_pSearchPane, &fEnable);
	pCmdUI->Enable( fEnable );
}

void CSearchFrame::OnHelp()
{
	NetHelp(m_helpString);
}

void CSearchFrame::OnUpdateHelp( CCmdUI *pCmdUI )
{
		pCmdUI->Enable( TRUE );
}

void CSearchFrame::OnFind()
{
	if ( m_bSearching ) {
		// We've turned into stop button
		XP_InterruptContext( GetContext() );
		return;
	}

	// Build Search

	ShowResults( FALSE );

	MSG_SearchFree (m_pSearchPane);
	MSG_SearchAlloc (m_pSearchPane);

	ASSERT(m_pOutliner);
	m_pOutliner->Invalidate();
					   
	CComboBox *combo;

	int iCurSel;
	MSG_ScopeAttribute scope;
	
	MSG_Master *master = WFE_MSGGetMaster();

	combo = (CComboBox *) m_barSearch.GetDlgItem( IDC_COMBO_SCOPE );
	iCurSel = combo->GetCurSel();

	DWORD dwData = combo->GetItemData(iCurSel);
	scope = m_barSearch.DetermineScope(dwData);
	
	MSG_AddScopeTerm( m_pSearchPane, scope, (MSG_FolderInfo *) dwData );

	m_barSearch.BuildQuery (m_pSearchPane);

	if (MSG_Search(m_pSearchPane) == SearchError_Success) {
		m_bSearching = TRUE;
		ShowResults( TRUE );
		SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		m_barStatus.SetPaneText( m_barStatus.CommandToIndex( ID_SEPARATOR), 
								 szLoadString( IDS_SEARCHING ) );
	}
}

void CSearchFrame::OnUpdateFind( CCmdUI *pCmdUI )
{
	CString cs;
	if ( m_bSearching || CanAllInterrupt()) {
		cs.LoadString( IDS_STOP );
	} else {
		cs.LoadString( IDS_SEARCH );
	}

	pCmdUI->SetText( cs );
}

void CSearchFrame::OnUpdateAndOr(CCmdUI *pCmdUI)
{

	if ( m_bSearching || CanAllInterrupt()) {	 
		pCmdUI->Enable(FALSE);		
	}else {
		pCmdUI->Enable(TRUE); 
	}
}

void CSearchFrame::OnTo()
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);

	if (count == 1) {
		MSG_ResultElement *elem = NULL;
		MSG_GetResultElement(m_pSearchPane, indices[0], &elem);

		ASSERT(elem);

		if ( !elem )
			return;

		MSG_SearchValue *value;
		MSG_GetResultAttribute(elem, attribMessageKey, &value);
		MessageKey key = value->u.key;
		MSG_GetResultAttribute(elem, attribFolderInfo, &value);
		MSG_FolderInfo *folderInfo = value->u.folder;

		if (folderInfo)
		{
			C3PaneMailFrame::Open(folderInfo, key);
		}
	}
}

void CSearchFrame::OnUpdateTo( CCmdUI *pCmdUI )
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);
	int nLines = m_pOutliner->GetTotalLines();
	pCmdUI->Enable( ((nLines > 0) && count) && MSG_GoToFolderStatus (m_pSearchPane, indices, count));
}

void CSearchFrame::OnUpdateQuery( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( !m_bSearching );
}

void CSearchFrame::OnScope()
{
	m_barSearch.UpdateAttribList();
	m_barSearch.UpdateOpList();
}

void CSearchFrame::OnAttrib1()
{
	EditHeader(0);
	m_barSearch.UpdateOpList();

}

void CSearchFrame::OnAttrib2()
{
	EditHeader(1);
	m_barSearch.UpdateOpList();
}

void CSearchFrame::OnAttrib3()
{
	EditHeader(2);
	m_barSearch.UpdateOpList();
}

void CSearchFrame::OnAttrib4()
{
	EditHeader(3);
	m_barSearch.UpdateOpList();
}

void CSearchFrame::OnAttrib5()
{
	EditHeader(4);
	m_barSearch.UpdateOpList();
}


void CSearchFrame::EditHeader(int iRow)
{
	//We are being asked to modify custom headers
	CComboBox *combo;	
	int iCurSel;
	MSG_SearchAttribute attrib;
	m_iRowSelected = iRow;
	combo = (CComboBox *) m_barSearch.m_searchObj.GetColumnOneAttributeWidget(iRow);
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
				::MessageBox(FEU_GetLastActiveFrame()->GetSafeHwnd(),
						 szLoadString(IDS_EDIT_HEADER_IN_USE), 
						 szLoadString(IDS_CUSTOM_HEADER_ERROR), 
						 MB_OK|MB_ICONSTOP);
				combo->SetCurSel(0);
				return;
				//We can't edit anything since another window already has the semaphore.
			}
		}
		else
		{	//Something is hosed!
			return;
		}

		m_pCustomHeadersDlg = new CCustomHeadersDlg(this);
		if (m_pCustomHeadersDlg)
		{
			m_pCustomHeadersDlg->ShowWindow(SW_SHOW);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// CSearchOutliner

CSearchOutliner::CSearchOutliner ( )
{
	m_attribSortBy = attribDate;
	m_bSortDescending = FALSE;
	m_iMysticPlane = 0;

    ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
		m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
		ASSERT(m_pIUserImage);
		m_pIUserImage->Initialize(IDB_MAILNEWS,16,16);
    }
	m_hFont = NULL;
}

CSearchOutliner::~CSearchOutliner ( )
{
    if (m_pUnkUserImage) {
		if (m_pIUserImage)
			m_pUnkUserImage->Release();
    }
	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
}

BEGIN_MESSAGE_MAP(CSearchOutliner, CMSelectOutliner )
	//{{AFX_MSG_MAP(CSearchOutliner)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CSearchOutliner::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = COutliner::OnCreate ( lpCreateStruct );
    
    InitializeClipFormats ( );

    return iRetVal;
}


void CSearchOutliner::PropertyMenu ( int iSel, UINT flags )	
{
    HMENU hmenu = CreatePopupMenu();

	if ( !hmenu )
		return; // Bail

	if ( iSel < m_iTotalLines ) 
	{
		::AppendMenu( hmenu, MF_STRING, ID_FILE_OPENMESSAGE, szLoadString( IDS_POPUP_OPENMESSAGE ) );
		::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );

		HMENU hFileMenu = CreatePopupMenu();
		UINT nID = FIRST_MOVE_MENU_ID;
		CMailNewsFrame::UpdateMenu( NULL, hFileMenu, nID );
		::AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT) hFileMenu, szLoadString( IDS_POPUP_FILE ) );
		::AppendMenu( hmenu, MF_STRING, CASTUINT(ID_EDIT_DELETEMESSAGE), szLoadString( IDS_POPUP_DELETEMESSAGE ) );
    }

    //	Track the popup now.
    POINT pt = m_ptHit;
    ClientToScreen(&pt);

   	::TrackPopupMenu( hmenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
					  GetParentFrame()->GetSafeHwnd(), NULL);

    //  Cleanup
    VERIFY(::DestroyMenu( hmenu ));
}


void CSearchOutliner::InitializeClipFormats(void)
{
    m_cfSearchMessages = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_SEARCH_FORMAT);
}




CLIPFORMAT * CSearchOutliner::GetClipFormatList(void)
{
    static CLIPFORMAT cfFormatList[2];
    cfFormatList[0] = m_cfSearchMessages;
    cfFormatList[1] = 0;

    return cfFormatList;
}

COleDataSource * CSearchOutliner::GetDataSource(void)
{
	MSG_ViewIndex *indices;
	int count;
	GetSelection(indices, count);

    HANDLE hContent = GlobalAlloc(GMEM_ZEROINIT|GMEM_MOVEABLE|GMEM_DDESHARE,sizeof(MailNewsDragData));
	MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock (hContent);

	pDragData->m_pane = m_pSearchPane;
	pDragData->m_indices = indices;
	pDragData->m_count = count;

    GlobalUnlock(hContent);

    COleDataSource * pDataSource = new COleDataSource;  
    pDataSource->CacheGlobalData(m_cfSearchMessages,hContent);

	if ( count == 1 ) {
		MSG_ResultElement *elem = NULL;
		MSG_GetResultElement(m_pSearchPane, indices[0], &elem);
		MSG_SearchValue *value;
		MSG_GetResultAttribute(elem, attribMessageKey, &value);
		MessageKey key = value->u.key;
		MSG_GetResultAttribute(elem, attribSubject, &value);
		URL_Struct *url = MSG_ConstructUrlForMessage( m_pSearchPane, key );

		if ( url ) {			
			RDFGLOBAL_DragTitleAndURL( pDataSource, value->u.string, url->address );
			NET_FreeURLStruct( url );
		}
	}

    return pDataSource;
}


BOOL CSearchOutliner::DeleteItem ( int iLine )
{
#ifndef DEBUG_phil
	// Delete? Are you kidding?
	MessageBeep(0);
    return FALSE;
#else
	MSG_ResultElement *elem = NULL;
	MSG_GetResultElement (m_pSearchPane, iLine, &elem);
	char *fileName = wfe_GetExistingFileName(m_hWnd, szLoadString(IDS_FILETOATTACH), ALL, TRUE);
	if (fileName)
	{
		CString cs;
		WFE_ConvertFile2Url(cs,(const char *)fileName);
		MSG_SearchValue value;
		value.attribute = attribJpegFile;
		value.u.string = XP_STRDUP(cs);
		MSG_ModifyLdapResult (elem, &value);

		XP_FREE (fileName);
		XP_FREE(value.u.string);
	}
	return TRUE;
#endif
}

void CSearchOutliner::ChangeResults (int num)
{
	int iOldTotal = m_iTotalLines;
	m_iTotalLines += num;
	EnableScrollBars();
	if (num > 0)
		HandleInsert(iOldTotal, num);
	else
		HandleDelete(iOldTotal, -num);
}

HFONT CSearchOutliner::GetLineFont(void *pLineData)
{
	MSG_SearchValue *folderInfoResult;
	MSG_GetResultAttribute( (MSG_ResultElement *) pLineData, attribFolderInfo, 
							&folderInfoResult ); 
	int16 doc_csid = 0;
	if (folderInfoResult)
		doc_csid = MSG_GetFolderCSID( folderInfoResult->u.folder );
	if (!doc_csid)
		doc_csid = INTL_DefaultWinCharSetID(0);
	int16 win_csid = INTL_DocToWinCharSetID(doc_csid);

	if ( win_csid != m_iCSID ) {
		m_iCSID = win_csid;
	    CClientDC dc( this );

 		if (m_hFont) {
			theApp.ReleaseAppFont(m_hFont);
		}

		LOGFONT lf;
		memset(&lf, 0, sizeof(LOGFONT));
		lf.lfPitchAndFamily = FF_SWISS;
		lf.lfCharSet = IntlGetLfCharset(win_csid);
		if (win_csid == CS_LATIN1)
 			_tcscpy(lf.lfFaceName, "MS Sans Serif");
		else
 			_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(win_csid));
   		lf.lfHeight = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
		m_hFont = theApp.CreateAppFont( lf );
	}
	return m_hFont ? m_hFont : m_hRegFont;
}

int CSearchOutliner::TranslateIcon (void * pData)
{
	// Do something smart to differentiate between mail and news
    int idxImage = IDX_MAILMESSAGE;
    return idxImage;
}

int CSearchOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}

void * CSearchOutliner::AcquireLineData ( int line )
{
	MSG_ResultElement *elem = NULL;
	MSG_GetResultElement (m_pSearchPane, line, &elem);
	return elem;
}

void CSearchOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, 
									OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}

void CSearchOutliner::ReleaseLineData ( void * )
{
}

LPCTSTR CSearchOutliner::GetColumnText ( UINT iColumn, void * pLineData )
{
	MSG_SearchValue *result;
	MSG_SearchAttribute attrib = (MSG_SearchAttribute) iColumn;
	CString cs;

	szResultText[0] = '\0'; // default to empty string
	szResultText[63] = '\0';

	if (MSG_GetResultAttribute( (MSG_ResultElement *) pLineData, attrib, &result) == 
		SearchError_Success) {

		switch (result->attribute) {
		case attribDate:
			_tcsncpy(szResultText, MSG_FormatDate(m_pSearchPane, result->u.date), 63);
			break;
		case attribPriority:
			MSG_GetPriorityName( result->u.priority, szResultText, 64);
			break;
		case attribMsgStatus:
			MSG_GetStatusName( result->u.msgStatus, szResultText, 64);
			break;
		case attribSender:
		case attribSubject:
			{
				char *buf = IntlDecodeMimePartIIStr(result->u.string, INTL_DocToWinCharSetID(m_iCSID), FALSE);
				if (buf) {
					_tcsncpy(szResultText, buf, 63);
					XP_FREE(buf);
					break;
				}
			}
		default:
			_tcsncpy(szResultText, result->u.string, 63);
			break;
		}
		MSG_DestroySearchValue (result);
	}
	return szResultText;
}

void CSearchOutliner::OnSelDblClk()
{
	BOOL bReuse = g_MsgPrefs.m_bMessageReuse;
	if (GetKeyState(VK_MENU) & 0x8000)
		bReuse = !bReuse;

	MSG_ViewIndex *indices;
	int i, count;
	
	GetSelection(indices, count);

	for ( i = 0; i < count; i++ ) {
		MSG_ResultElement *elem = NULL;
		MSG_GetResultElement(m_pSearchPane, indices[i], &elem);

		ASSERT(elem);

		if ( !elem )
			continue;

		MWContextType cxType = MSG_GetResultElementType( elem );

		if ( cxType == MWContextMail || cxType == MWContextMailMsg || 
			 cxType == MWContextNews || cxType == MWContextNewsMsg ) {
			CMessageFrame *frame = CMessageFrame::Open ();
			MSG_OpenResultElement (elem, frame->GetPane());
		} else if ( cxType == MWContextBrowser ) {
			MWContext *pContext = NULL;
			VERIFY(pContext = CFE_CreateNewDocWindow( NULL, NULL ));
			if (pContext) {
				MSG_OpenResultElement( elem, (MSG_Pane *) pContext );
			}
		} else { 
			ASSERT(0); // What on earth are you passing me?
		}
	}
}

void CSearchOutliner::OnSelChanged()
{
#ifndef _WIN32 // Force update on Win16
	GetParentFrame()->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
#endif
}


void CSearchOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CSearchOutliner::MysticStuffFinishing( XP_Bool asynchronous,
											 MSG_NOTIFY_CODE notify, 
											 MSG_ViewIndex where,
											 int32 num )
{
	if (notify == MSG_NotifyInsertOrDelete)
	{
		if (num > 0)
			HandleInsert(where, num);
		else
			HandleDelete(where, -num);
		((CSearchFrame*)GetParentFrame())->ModalStatusEnd();
	}	
}


/////////////////////////////////////////////////////////////////////////////
// CSearchOutlinerParent

BOOL CSearchOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, LPCTSTR text )
{
	MSG_SearchAttribute attrib = (MSG_SearchAttribute) idColumn;
    CSearchOutliner *pOutliner = (CSearchOutliner *) m_pOutliner;
	
	// Draw Sort Indicator
	MSG_COMMAND_CHECK_STATE sortType = pOutliner->m_attribSortBy == attrib ? MSG_Checked : MSG_Unchecked;

	if (sortType != MSG_Checked)
		return FALSE;

	int idxImage = pOutliner->m_bSortDescending ? IDX_SORTINDICATORUP : IDX_SORTINDICATORDOWN;

	UINT dwDTFormat = DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
	RECT rectText = rect;
	rectText.left += 4;
	rectText.right -= 5;

	rectText.right -= 14;
    m_pIImage->DrawTransImage( idxImage,
							   rectText.right + 4,
							   (rect.top + rect.bottom) / 2 - 4,
							   &dc );

	WFE_DrawTextEx( 0, dc.m_hDC, (LPTSTR) text, -1, 
					&rectText, dwDTFormat, WFE_DT_CROPRIGHT );

    return TRUE;
}

COutliner * CSearchOutlinerParent::GetOutliner ( void )
{
    return new CSearchOutliner;
}

void CSearchOutlinerParent::CreateColumns ( void )
{
	CString cs;

	cs.LoadString(IDS_MAIL_SUBJECT);
    m_pOutliner->AddColumn ( cs, attribSubject,  20, 0, ColumnVariable, 3500 ); 
	cs.LoadString(IDS_MAIL_SENDER);
    m_pOutliner->AddColumn ( cs, attribSender,   20, 0, ColumnVariable, 2500 ); 
	cs.LoadString(IDS_MAIL_DATE);
    m_pOutliner->AddColumn ( cs, attribDate,     20, 0, ColumnVariable, 1000 );
	cs.LoadString(IDS_MAIL_PRIORITY);
    m_pOutliner->AddColumn ( cs, attribPriority, 20, 0, ColumnVariable, 1000 );
	cs.LoadString(IDS_MAIL_LOCATION);
    m_pOutliner->AddColumn ( cs, attribLocation, 20, 0, ColumnVariable, 2000 );
	m_pOutliner->SetImageColumn( attribSubject );
	m_pOutliner->SetHasPipes( FALSE );
}

BOOL CSearchOutlinerParent::ColumnCommand ( int idColumn )
{
	MSG_SearchAttribute attrib = (MSG_SearchAttribute) idColumn;
    CSearchOutliner *pOutliner = (CSearchOutliner *) m_pOutliner;
	
	if (attrib == pOutliner->m_attribSortBy) {
		pOutliner->m_bSortDescending = !pOutliner->m_bSortDescending; 
	} else {
		pOutliner->m_attribSortBy = attrib;
	}
	MSG_SortResultList(pOutliner->m_pSearchPane, pOutliner->m_attribSortBy, pOutliner->m_bSortDescending);
	Invalidate();
	pOutliner->Invalidate();

	return TRUE;
}

void CSearchFrame::Open()
{
	if (!g_pSearchWindow) {
		g_pSearchWindow = new CSearchFrame();
		g_pSearchWindow->Create();
	} else {
		g_pSearchWindow->ActivateFrame( g_pSearchWindow->IsIconic() ? SW_RESTORE : SW_SHOW );
	}       
}

void CSearchFrame::Open( CMailNewsFrame *pFrame )
{
	CSearchFrame::Open();
	if ( g_pSearchWindow ) {
		g_pSearchWindow->UpdateScopes( pFrame );
	}
}

void CSearchFrame::Close()
{
	if (g_pSearchWindow) {
		g_pSearchWindow->PostMessage(WM_CLOSE);
	}       
}

/////////////////////////////////////////////////////////////////////////////
// CLDAPSearchFrame

void CLDAPSearchFrame::Create()
{
	m_bIsLDAPSearch = TRUE;

	DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN;
	CString strFullString, strTitle;
	strFullString.LoadString( IDR_SEARCHFRAME );
	AfxExtractSubString( strTitle, strFullString, 0 );

	LPCTSTR lpszClass = GetIconWndClass( dwDefaultStyle, IDR_SEARCHFRAME );
	LPCTSTR lpszTitle = strTitle;
	CFrameWnd::Create(lpszClass, lpszTitle, dwDefaultStyle);

	ActivateFrame();
}

BOOL CLDAPSearchFrame::OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext )
{
	CWnd *pWnd;
	CCreateContext Context;
	Context.m_pNewViewClass = RUNTIME_CLASS(CLDAPSearchView);
	
	if ( pWnd = CreateView(&Context) ) {
		COutlinerView *pView = (COutlinerView *) pWnd;
		pView->CreateColumns ( );
		m_pOutliner = (CLDAPSearchOutliner *) pView->m_pOutlinerParent->m_pOutliner;
		m_pOutliner->SetContext( GetContext() );
		m_pOutliner->SetPane (m_pSearchPane);
	} else {
		return FALSE;
	}
	return TRUE;
}

BEGIN_MESSAGE_MAP(CLDAPSearchFrame, CSearchFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_CBN_SELCHANGE(IDC_COMBO_SCOPE, OnScope)
	ON_BN_CLICKED(IDC_FIND, OnFind)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_UPDATE_COMMAND_UI(IDC_ADD, OnUpdateAdd)
	ON_BN_CLICKED(IDC_TO, OnTo)
	ON_UPDATE_COMMAND_UI(IDC_TO, OnUpdateTo)
END_MESSAGE_MAP()

int CLDAPSearchFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	UINT aIDArray[] = { ID_SEPARATOR, IDS_TRANSFER_STATUS };
	int res = CFrameWnd::OnCreate(lpCreateStruct);

	m_barSearch.m_bLDAP = TRUE;

	m_helpString = HELP_SEARCH_LDAP;

#ifdef _WIN32
	m_barSearch.Create( this, IDD_SEARCHLDAP, WS_CHILD|CBRS_ALIGN_TOP, 1 );
#else
	m_barSearch.Create( this, IDD_SEARCHLDAP, WS_CHILD|CBRS_TOP, 1 );
#endif
	m_barStatus.Create( this, FALSE, FALSE );
	m_barAction.Create( this, IDD_LDAPSEARCH, WS_CHILD|CBRS_BOTTOM, 2);

	m_barStatus.SetPaneText(m_barStatus.CommandToIndex( ID_SEPARATOR), 
							szLoadString(IDS_SEARCHLDAP));

	RecalcLayout( );
	
	m_barSearch.InitializeAttributes (widgetText, attribCommonName);

	CComboBox *combo;
    combo = (CComboBox *) m_barSearch.GetDlgItem(IDC_COMBO_SCOPE);
	combo->ResetContent();

	XP_List *ldapDirectories = XP_ListNew();
	if (ldapDirectories) {
		DIR_GetLdapServers (theApp.m_directories, ldapDirectories);
		if (XP_ListCount(ldapDirectories)) {
			for (int i = 1; i <= XP_ListCount(ldapDirectories); i++) {
				DIR_Server *server = (DIR_Server*) XP_ListGetObjectNum (ldapDirectories, i);
				XP_ASSERT(server);
				if (server)     {
					if (server->description && server->description[0])
						combo->AddString (server->description);
					else
						combo->AddString (server->serverName);
					combo->SetItemData (i-1, scopeLdapDirectory * 2);
				}
			}
			XP_ListDestroy (ldapDirectories);
		}
		else {
			combo->AddString("");
			combo->SetItemData (0, scopeLdapDirectory * 2);
		}
	}
	else {
		combo->AddString("");
		combo->SetItemData (0, scopeLdapDirectory * 2);
	}

	combo->SetCurSel(0);
	OnScope();

	// Initially size window to only dialog + title bar.

	CRect rect, rect2;
	int BorderX = GetSystemMetrics(SM_CXFRAME);
	int BorderY = GetSystemMetrics(SM_CYFRAME);

	GetWindowRect(&rect);
	m_barSearch.GetWindowRect(&rect2);
	CSize size = m_barSearch.CalcFixedLayout(FALSE, FALSE);
	// Figure height of title bar + bottom border
	size.cy += rect2.top - rect.top + BorderY;
	size.cx += BorderX * 2;
	SetWindowPos( NULL, 0, 0, size.cx, size.cy,
				  SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);

	m_iHeight = size.cy;
	m_iWidth = size.cx;

	OnNew();

	ShowResults( TRUE );

	return res;
}

void CLDAPSearchFrame::OnClose()
{
	if (m_pOutliner)
		m_pOutliner->SaveXPPrefs("mailnews.ldapsearch_columns_win");
	CFrameWnd::OnClose();
	g_pLDAPSearchWindow = NULL;
}

void CLDAPSearchFrame::OnScope()
{
	CSearchFrame::OnScope();

	CComboBox *combo = (CComboBox *) m_barSearch.GetDlgItem(IDC_COMBO_SCOPE);
	int iCurSel = combo->GetCurSel();

	XP_List *ldapDirectories = XP_ListNew();
	if (!ldapDirectories)
		return;

	DIR_GetLdapServers(theApp.m_directories, ldapDirectories);
	DIR_Server *pServer = (DIR_Server*) XP_ListGetObjectNum(ldapDirectories, iCurSel + 1);
	XP_ListDestroy (ldapDirectories);

	if (!pServer)
		return;

	int iCount = m_pOutliner->GetNumColumns();
	for (int i = 0; i < iCount; i++) {
		MSG_SearchAttribute attrib = (MSG_SearchAttribute) m_pOutliner->GetColumnAtPos(i);
		DIR_AttributeId id;
		MSG_SearchAttribToDirAttrib(attrib, &id);
		const char *text = DIR_GetAttributeName(pServer, id);
		m_pOutliner->SetColumnName(attrib, text);
	}
	m_pOutliner->GetParent()->Invalidate();
}

void CLDAPSearchFrame::OnFind()
{
	if ( m_bSearching ) {
		// We've turned into stop button
		XP_InterruptContext( GetContext() );
		m_bSearching = FALSE;
		return;
	} else if (CanAllInterrupt()) {
		AllInterrupt();
	}

	// Build Search

	ShowResults( FALSE );

	MSG_SearchFree (m_pSearchPane);
	MSG_SearchAlloc (m_pSearchPane);

	ASSERT(m_pOutliner);
	m_pOutliner->Invalidate();
					   
	CComboBox *combo;

	int iCurSel;
	
	combo = (CComboBox *) m_barSearch.GetDlgItem( IDC_COMBO_SCOPE );
	iCurSel = combo->GetCurSel();

	XP_List *ldapDirectories = XP_ListNew();
	if (!ldapDirectories)
		return;
	DIR_GetLdapServers (theApp.m_directories, ldapDirectories);
	DIR_Server *pServer = (DIR_Server*) XP_ListGetObjectNum (ldapDirectories, iCurSel + 1);
	XP_ListDestroy (ldapDirectories);
	if (!pServer)
		return;

	MSG_AddLdapScope( m_pSearchPane, pServer);

	m_barSearch.BuildQuery (m_pSearchPane);

	if (MSG_Search(m_pSearchPane) == SearchError_Success) {
		m_bSearching = TRUE;
		ShowResults( TRUE );
		SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		m_barStatus.SetPaneText(m_barStatus.CommandToIndex( ID_SEPARATOR), 
								szLoadString( IDS_SEARCHING ) );
	}
}

void CLDAPSearchFrame::OnAdd()
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );
	MSG_AddLdapResultsToAddressBook( m_pSearchPane, indices, count );
}

void CLDAPSearchFrame::OnUpdateAdd( CCmdUI *pCmdUI )
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );
	pCmdUI->Enable( count > 0 && !m_bSearching);
}

void CLDAPSearchFrame::OnTo()
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );
	MSG_ComposeFromLdapResults( m_pSearchPane, indices, count);
}

void CLDAPSearchFrame::OnUpdateTo( CCmdUI *pCmdUI )
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );
	pCmdUI->Enable( count > 0 && !m_bSearching);
}

/////////////////////////////////////////////////////////////////////////////
// CLDAPSearchOutliner

BEGIN_MESSAGE_MAP(CLDAPSearchOutliner, CSearchOutliner)
	//{{AFX_MSG_MAP(CLDAPSearchOutliner)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CLDAPSearchOutliner::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = CSearchOutliner::OnCreate ( lpCreateStruct );
    
    InitializeClipFormats ( );

    return iRetVal;
}

HFONT CLDAPSearchOutliner::GetLineFont( void *pData )
{
	return m_hRegFont;
}

int CLDAPSearchOutliner::TranslateIcon (void * pData)
{
	// Do something smart
    int idxImage = IDX_MAILMESSAGE;
    return idxImage;
}

void CLDAPSearchOutliner::InitializeClipFormats(void)
{
     m_cfAddresses = (CLIPFORMAT)RegisterClipboardFormat(vCardClipboardFormat);
	 m_cfSourceTarget = (CLIPFORMAT)RegisterClipboardFormat(LDAPSEARCH_SOURCETARGET_FORMAT);
}

CLIPFORMAT * CLDAPSearchOutliner::GetClipFormatList(void)
{
    static CLIPFORMAT cfFormatList[3];
    cfFormatList[0] = m_cfAddresses;
	cfFormatList[1] = m_cfSourceTarget;
    cfFormatList[2] = 0;
    return cfFormatList;
}

COleDataSource * CLDAPSearchOutliner::GetDataSource(void)
{
    COleDataSource * pDataSource = new COleDataSource;
	char* pVcard = NULL;
	char* pVcards = XP_STRDUP("");
	HANDLE hString = 0;
	PersonEntry person;
	char szFirstNameText[130] = "\0";
	char szLastNameText[130] = "\0";
	char szEmailText[130] = "\0";
	char szOrganizationText[130] = "\0";
	char szLocalityText[130] = "\0";
	char szWorkPhoneText[130] = "\0";
	MSG_ResultElement* elem = NULL;
	MSG_SearchValue *result = NULL;
	MSG_ViewIndex *indices;
	int count;

	GetSelection( indices, count );

	for (int i = 0; i < count; i++){

		szFirstNameText[0] = '\0';
		szLastNameText[0] = '\0';
		szEmailText[0] = '\0';
		szOrganizationText[0] = '\0';
		szLocalityText[0] = '\0';
		szWorkPhoneText[0] = '\0';

		if (MSG_GetResultElement (m_pSearchPane, indices[i], &elem) == SearchError_Success) {
			if (MSG_GetResultAttribute( elem, attribGivenName, &result) == SearchError_Success) {
				XP_STRNCPY_SAFE (szFirstNameText, result->u.string, sizeof (szFirstNameText));
				MSG_DestroySearchValue (result);
			}
			if (MSG_GetResultAttribute( elem, attribSurname, &result) == SearchError_Success) {
				XP_STRNCPY_SAFE (szLastNameText, result->u.string, sizeof (szLastNameText));
				MSG_DestroySearchValue (result);
			}
			if (MSG_GetResultAttribute( elem, attrib822Address, &result) == SearchError_Success)  {
				XP_STRNCPY_SAFE (szEmailText, result->u.string, sizeof (szEmailText));
				MSG_DestroySearchValue (result);
			}
			if (MSG_GetResultAttribute( elem, attribOrganization, &result) == SearchError_Success) {
				XP_STRNCPY_SAFE (szOrganizationText, result->u.string, sizeof (szOrganizationText));
				MSG_DestroySearchValue (result);
			}
			if (MSG_GetResultAttribute( elem, attribLocality, &result) == SearchError_Success) {
				XP_STRNCPY_SAFE (szLocalityText, result->u.string, sizeof (szLocalityText));
				MSG_DestroySearchValue (result);
			}
			if (MSG_GetResultAttribute( elem, attribPhoneNumber, &result) == SearchError_Success) {
				XP_STRNCPY_SAFE (szWorkPhoneText, result->u.string, sizeof (szLocalityText));
				MSG_DestroySearchValue (result);
			}

			if ((XP_STRLEN (szFirstNameText) == 0) || (XP_STRLEN (szLastNameText) == 0))
			{
				if (MSG_GetResultAttribute( elem, attribCommonName, &result) == SearchError_Success) {
					XP_STRNCPY_SAFE (szFirstNameText, result->u.string, sizeof (szFirstNameText));
					MSG_DestroySearchValue (result);
					szLastNameText[0] = '\0';
				}
			}
		}
		person.Initialize();
		if (szFirstNameText[0] != '\0')
			person.pGivenName = szFirstNameText;
		if (szLastNameText[0] != '\0')
			person.pFamilyName = szLastNameText;
		if (szOrganizationText[0] != '\0')
			person.pCompanyName = szOrganizationText;
		if (szLocalityText[0] != '\0')
			person.pLocality = szLocalityText;
		if (szEmailText[0] != '\0')
			person.pEmailAddress = szEmailText;
		if (szWorkPhoneText[0] != '\0')
			person.pWorkPhone = szWorkPhoneText;

		AB_ExportToVCardFromPerson(theApp.m_pABook, &person, &pVcard);
		pVcards = StrAllocCat(pVcards, pVcard);
		XP_FREE(pVcard);
		pVcard = NULL;

	}

	if (pVcards) {
		hString = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,strlen(pVcards)+1);
        LPSTR lpszString = (LPSTR)GlobalLock(hString);
        strcpy(lpszString,pVcards);
		XP_FREE (pVcards);
        GlobalUnlock(hString);
        pDataSource->CacheGlobalData(m_cfAddresses, hString);
		pDataSource->CacheGlobalData(m_cfSourceTarget, hString);
		pDataSource->CacheGlobalData(CF_TEXT, hString);
    }

    return pDataSource;
}

/////////////////////////////////////////////////////////////////////////////
// CSearchOutlinerParent

COutliner * CLDAPSearchOutlinerParent::GetOutliner ( void )
{
    return new CLDAPSearchOutliner;
}

void CLDAPSearchOutlinerParent::CreateColumns ( void )
{
	CString cs;

	cs.LoadString(IDS_USERNAME);
    m_pOutliner->AddColumn (cs,			attribCommonName,		20, 0, ColumnVariable, 2500 );
	cs.LoadString(IDS_EMAILADDRESS);
    m_pOutliner->AddColumn (cs,			attrib822Address,		20, 0, ColumnVariable, 2000 ); 
	cs.LoadString(IDS_COMPANYNAME);
    m_pOutliner->AddColumn (cs,			attribOrganization,		20, 0, ColumnVariable, 2000 ); 
	cs.LoadString(IDS_PHONE);
    m_pOutliner->AddColumn (cs,			attribPhoneNumber,		20, 0, ColumnVariable, 2000, FALSE);
	cs.LoadString(IDS_LOCALITY);
    m_pOutliner->AddColumn (cs,			attribLocality,			20, 0, ColumnVariable, 1500 );
	m_pOutliner->SetImageColumn( attribSubject );
	m_pOutliner->SetHasPipes( FALSE );

	m_pOutliner->SetVisibleColumns(DEF_VISIBLE_COLUMNS);
	m_pOutliner->LoadXPPrefs("mailnews.ldapsearch_columns_win");
}

void CLDAPSearchFrame::Open()
{
	XP_List *ldapDirectories = XP_ListNew();
	if (!ldapDirectories)
		return;

	DIR_GetLdapServers(theApp.m_directories, ldapDirectories);
	if (XP_ListCount(ldapDirectories)) {
		if (!g_pLDAPSearchWindow) {
			g_pLDAPSearchWindow = new CLDAPSearchFrame();
			g_pLDAPSearchWindow->Create();
		} else {
			g_pLDAPSearchWindow->ActivateFrame( g_pLDAPSearchWindow->IsIconic() ? SW_RESTORE : SW_SHOW );
		}
	} else {
		::MessageBox(FEU_GetLastActiveFrame()->GetSafeHwnd(),
					 szLoadString(IDS_NOLDAPSERVERS), 
					 szLoadString(IDS_TITLE_ERROR), 
					 MB_OK|MB_ICONSTOP);
	}
	XP_ListDestroy (ldapDirectories);
}

void CLDAPSearchFrame::Close()
{
	if (g_pLDAPSearchWindow) {
		g_pLDAPSearchWindow->PostMessage(WM_CLOSE);
	}       
}

void WFE_MSGOpenSearch()
{
	CSearchFrame::Open();
}

void WFE_MSGSearchClose()
{
	CSearchFrame::Close();
}
	
void WFE_MSGOpenLDAPSearch()
{
	CLDAPSearchFrame::Open();
}

void WFE_MSGLDAPSearchClose()
{
    CLDAPSearchFrame::Close();
}
