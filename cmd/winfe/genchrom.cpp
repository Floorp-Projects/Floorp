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
#include "widgetry.h"
#include "genchrom.h"
#include "toolbar.cpp"
#include "prefapi.h"
#include "navfram.h"

#include "wfedde.h" //JOKI

#define MAIN_PICT_HEIGHT   21
#define MAIN_PICT_WIDTH    23

static SIZE sizeBitmapDefault = { 23, 21 };

// these button sizes INCLUDE HIGHLIGHT_WIDTH on each edge
static SIZE sizeNoviceButton = { 50, 40 };
// according to CToolBar::SetSizes(): "button must be big enough
// to hold image + 3 pixels on each side"
static SIZE sizeAdvancedButton = { 30, 27 };

/////////////////////////////////////////////////////////////////////
// CGenericToolBar

CGenericToolBar::CGenericToolBar( LPUNKNOWN pOuterUnk )
{
	m_ulRefCount = 0;
	m_pOuterUnk = pOuterUnk;
	m_nBitmapID = 0;
	m_pCommandToolbar = NULL;
}

CGenericToolBar::~CGenericToolBar()
{
	if(m_pCommandToolbar)
		delete m_pCommandToolbar;
}

STDMETHODIMP CGenericToolBar::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	if ( m_pOuterUnk )
		return m_pOuterUnk->QueryInterface(refiid, ppv);
	else {
		*ppv = NULL;
		if (IsEqualIID(refiid,IID_IUnknown))
   			*ppv = (LPUNKNOWN) (LPNSTOOLBAR) this;
		else if (IsEqualIID(refiid, IID_INSToolBar))
			*ppv = (LPNSTOOLBAR) this;
		else if (IsEqualIID(refiid, IID_INSAnimation))
			*ppv = (LPNSANIMATION) this;

		if (*ppv != NULL) {
   			AddRef();
			return NOERROR;
		}
            
		return ResultFromScode(E_NOINTERFACE);
	}
}

STDMETHODIMP_(ULONG) CGenericToolBar::AddRef(void)
{
	if ( m_pOuterUnk )
		return m_pOuterUnk->AddRef();
	else
		return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CGenericToolBar::Release(void)
{
	if ( m_pOuterUnk )
		return m_pOuterUnk->Release();
	else {
		ULONG ulRef = --m_ulRefCount;
		if (m_ulRefCount == 0) {
			delete this;   	
		}
		return ulRef;
	}
}

int CGenericToolBar::Create( CFrameWnd *pParent, 
							 DWORD dwStyle, 
							 UINT nID )
{
#ifdef WIN32
	dwStyle |= CBRS_TOOLTIPS | CBRS_FLYBY;
#endif

//	int res =  m_barTool.Create( pParent, dwStyle, nID );
//	if ( res == -1 ) {
//		return res;
//	}

	m_pCommandToolbar = new CCommandToolbar(20, theApp.m_pToolbarStyle, 43, 27, 27);

	int res = m_pCommandToolbar->Create(pParent);
	

	if(res)
	{
		m_pCommandToolbar->ShowWindow(SW_SHOW);
		//m_barTool.SetToolbar(m_pCommandToolbar);
	}
	pParent->RecalcLayout();
	return res;
}

void CGenericToolBar::SetSizes( SIZE sizeButton, 
								SIZE sizeImage )
{
		m_pCommandToolbar->SetBitmapSize(sizeImage.cx, sizeImage.cy);
}

void CGenericToolBar::SetButtons( const UINT *lpIDArray,
								  int nIDCount, UINT nBitmapID )
{
	m_pCommandToolbar->RemoveAllButtons();

	if(nBitmapID != 0)
	{
		m_nBitmapID = nBitmapID;
		m_pCommandToolbar->SetBitmap(nBitmapID);
	}

	HBITMAP hBitmap = m_pCommandToolbar->GetBitmap();

	for(int i = 0; i < nIDCount; i++)
	{
		CCommandToolbarButton *pButton = new CCommandToolbarButton;

		CString statusStr, toolTipStr, textStr;

		WFE_ParseButtonString(lpIDArray[i], statusStr, toolTipStr, textStr);

		pButton->Create(m_pCommandToolbar, theApp.m_pToolbarStyle,
						CSize(sizeNoviceButton.cx , sizeNoviceButton.cy), 
						CSize(sizeAdvancedButton.cx, sizeAdvancedButton.cy),(const char*)textStr,
						(const char *)toolTipStr, (const char *)statusStr, m_nBitmapID, i,
						CSize(sizeBitmapDefault.cx , sizeBitmapDefault.cy), lpIDArray[i], SHOW_ALL_CHARACTERS);


		if(hBitmap != NULL)
			pButton->SetBitmap(hBitmap, TRUE);

		m_pCommandToolbar->AddButton(pButton);
	}
//	m_barTool.PlaceToolbar();
}
	
void CGenericToolBar::SetButtonStyle( UINT nIDButtonCommand, DWORD dwButtonStyle )
{
    CToolbarButton * pButton = m_pCommandToolbar->GetButton(nIDButtonCommand);
    if( pButton ){
        pButton->SetStyle(dwButtonStyle);
    }
}

void CGenericToolBar::GetButtonRect( UINT nIDButtonCommand, RECT * pRect)
{
    CToolbarButton * pButton = m_pCommandToolbar->GetButton(nIDButtonCommand);
    if( pButton ){
        pButton->GetWindowRect(pRect);
    }
}

void CGenericToolBar::AddButton( CToolbarButton *pButton, int index )
{
	m_pCommandToolbar->AddButton( pButton, index );
}

void CGenericToolBar::RemoveAllButtons()
{
	m_pCommandToolbar->RemoveAllButtons( );
}

CToolbarButton *CGenericToolBar::RemoveButton( int index )
{
	return m_pCommandToolbar->RemoveButton( index );
}

BOOL CGenericToolBar::LoadBitmap( LPCSTR lpszResourceName )
{
//	return m_barTool.LoadBitmap( lpszResourceName );
	m_nBitmapID = (UINT) LOWORD(lpszResourceName);

	m_pCommandToolbar->SetBitmap(m_nBitmapID);

	int nCount = m_pCommandToolbar->GetNumButtons();

	for(int i = 0; i < nCount; i++)
	{
		m_pCommandToolbar->GetNthButton(i)->ReplaceBitmap(m_pCommandToolbar->GetBitmap(), i, TRUE);
	}

	return 1;
}

void CGenericToolBar::SetToolbarStyle( int nToolbarStyle )
{
	if(m_pCommandToolbar)
	{
		m_pCommandToolbar->SetToolbarStyle(nToolbarStyle);
		m_pCommandToolbar->GetParentFrame()->RecalcLayout();
	//	m_barTool.RedrawWindow();
	}
}

void CGenericToolBar::Show( BOOL bShow )
{
	if(m_pCommandToolbar)
		m_pCommandToolbar->ShowWindow( bShow ? SW_SHOW : SW_HIDE );
}

void CGenericToolBar::SetButtonsSameWidth(BOOL bSameWidth)
{
	if(m_pCommandToolbar)
		m_pCommandToolbar->SetButtonsSameWidth(bSameWidth);
}

HWND CGenericToolBar::GetHWnd()
{
	if(m_pCommandToolbar){
        return m_pCommandToolbar->m_hWnd;
    }
    return (HWND)0;
}

void CGenericToolBar::StartAnimation( )
{
//	m_barTool.StartAnimation( );
}

void CGenericToolBar::StopAnimation( )
{
//	m_barTool.StopAnimation( );
}

/////////////////////////////////////////////////////////////////////
// CGenericStatusBar

class CModalStatus: public CDialog {
protected:

	CProgressMeter m_progressMeter;
	UINT m_idTimer;
	BOOL m_bInModal;

	enum { IDD = IDD_NEWMAIL };

	virtual BOOL OnInitDialog();
	virtual void OnCancel( );

	afx_msg void OnTimer( UINT nIDEvent );
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

public:
	CModalStatus();
	~CModalStatus( );

	BOOL Create( CWnd *pParentWnd, UINT uDelay );

	void SetPercentDone( int iPercent );
	void SetStatusText( LPCTSTR lpszStatus );
	void ProgressComplete();
};

CModalStatus::CModalStatus()
{
	m_idTimer = 0;
	m_bInModal = FALSE;
}

CModalStatus::~CModalStatus( )
{
}

BOOL CModalStatus::Create( CWnd *pParentWnd, UINT uDelay )
{
	BOOL res = CDialog::Create( IDD, pParentWnd );

	if ( res ) {
		m_idTimer = SetTimer( 1, uDelay, NULL );
	}
	return res;
}

BOOL CModalStatus::OnInitDialog( )
{
	CDialog::OnInitDialog();

	SetWindowText(szLoadString(IDS_PROGRESS));

	m_progressMeter.SubclassDlgItem( IDC_PROGRESS, this );

	return FALSE;
}

void CModalStatus::OnCancel()
{
	GetParentFrame()->PostMessage( WM_COMMAND, (WPARAM) ID_NAVIGATE_INTERRUPT, (LPARAM) 0 );
}

BEGIN_MESSAGE_MAP(CModalStatus, CDialog)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CModalStatus::OnTimer( UINT nIDEvent )
{
	KillTimer( m_idTimer );
	m_idTimer = 0;

	m_bInModal = TRUE;
	CWnd* pParentWnd = GetParentFrame();
	pParentWnd->EnableWindow(FALSE);
	ShowWindow(SW_SHOW);
	EnableWindow(TRUE);
	UpdateWindow();
}

void CModalStatus::OnDestroy()
{
	if (m_idTimer) {
		KillTimer( m_idTimer );
		m_idTimer = 0;
	}
}

void CModalStatus::SetStatusText( LPCTSTR lpszStatus )
{
	CWnd *widget = GetDlgItem( IDC_STATIC1 );
	widget->SetWindowText( lpszStatus );
}

void CModalStatus::SetPercentDone( int iPercent )
{
	CWnd *widget = GetDlgItem( IDC_STATIC2 );
	CString cs;
	cs.Format("%d%%", iPercent);
	widget->SetWindowText(cs);

	m_progressMeter.StepItTo( iPercent );
}

void CModalStatus::ProgressComplete()
{
	if (m_bInModal) {
	 	CWnd* pParentWnd = GetParentFrame();
		pParentWnd->EnableWindow(TRUE);
		if (::GetActiveWindow() == m_hWnd)
			pParentWnd->SetActiveWindow();
		m_bInModal = FALSE;
		DestroyWindow();
	} else {
		DestroyWindow();
	}
}

CGenericStatusBar::CGenericStatusBar( LPUNKNOWN pOuterUnk )
{
	m_ulRefCount = 0;
    
	m_pOuterUnk = pOuterUnk;
	m_pStatusBar = NULL;
	m_pCreatedBar = NULL;
    
	m_bModal = FALSE;
	m_pModalStatus = NULL;

	m_iProg = 0;
}

CGenericStatusBar::~CGenericStatusBar()
{
    //## NOTE: If we instantiate the status bar on the heap (via new), it is auto-deleted.
}

STDMETHODIMP CGenericStatusBar::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	if ( m_pOuterUnk )
		return m_pOuterUnk->QueryInterface(refiid, ppv);
	else {
		*ppv = NULL;
		if (IsEqualIID(refiid,IID_IUnknown))
   			*ppv = (LPUNKNOWN) this;
		else if (IsEqualIID(refiid, IID_INSStatusBar))
			*ppv = (LPNSSTATUSBAR) this;

		if (*ppv != NULL) {
   			AddRef();
			return NOERROR;
		}
            
		return ResultFromScode(E_NOINTERFACE);
	}
}

STDMETHODIMP_(ULONG) CGenericStatusBar::AddRef(void)
{
	if ( m_pOuterUnk )
		return m_pOuterUnk->AddRef();
	else
		return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CGenericStatusBar::Release(void)
{
	if ( m_pOuterUnk )
		return m_pOuterUnk->Release();
	else {
		ULONG ulRef = --m_ulRefCount;
		if (m_ulRefCount == 0) {
			delete this;   	
		}
		return ulRef;
	}
}

CNetscapeStatusBar *CGenericStatusBar::GetNetscapeStatusBar()
{
    return m_pStatusBar;
}

BOOL CGenericStatusBar::Create( CWnd* pParentWnd, DWORD dwStyle, UINT nID, BOOL bSecurityStatus /*=TRUE*/, BOOL bTaskbar /*=TRUE*/ )
{
	m_pCreatedBar = new CNetscapeStatusBar;

    if( !m_pCreatedBar->Create( pParentWnd, bSecurityStatus, bTaskbar ) )
	{
		delete m_pCreatedBar;
		m_pCreatedBar = NULL;

		TRACE("Failed to create status bar\n");

		return FALSE;      // fail to create
	}

	m_pStatusBar = m_pCreatedBar;
    m_pStatusBar->m_bAutoDelete = TRUE;

	m_pStatusBar->GetParentFrame()->RecalcLayout();

	return TRUE;
}

void CGenericStatusBar::Attach( CNetscapeStatusBar *pBar )
{
	m_pStatusBar = pBar;
}

void CGenericStatusBar::Show(BOOL bShow)
{
	if ( m_pStatusBar ) {
		m_pStatusBar->ShowWindow( bShow ? SW_SHOW : SW_HIDE);
		m_pStatusBar->GetParentFrame()->RecalcLayout();
	}
}

void CGenericStatusBar::ModalStatus( BOOL bModal, UINT uDelay, char * pszTitle )
{
	if ( bModal != m_bModal ) {
		m_bModal = bModal;
		if ( bModal ) {
			if (!m_pModalStatus && m_pStatusBar ) {
				m_pModalStatus = new CModalStatus;
				m_pModalStatus->Create( m_pStatusBar->GetParentFrame(), uDelay );
            if (pszTitle)
               m_pModalStatus->SetWindowText(pszTitle);
			}
		} else {
			if ( m_pModalStatus ) {
				m_pModalStatus->ProgressComplete();
				m_pModalStatus = NULL;
			}
		}
		SetStatusText( m_csStatus );
		SetProgress( m_iProg );
	}
}

void CGenericStatusBar::SetStatusText(const char * lpszText)
{
	m_csStatus = lpszText ? lpszText : "";

	if ( m_bModal ) {
		if ( m_pModalStatus ) {
			m_pModalStatus->SetStatusText( m_csStatus );
		}
	}
	if ( m_pStatusBar ) {
		m_pStatusBar->SetPaneText(m_pStatusBar->CommandToIndex(ID_SEPARATOR), m_csStatus);
		// WHS - if we're showing status for a foreground operation,
		// we need to manually repaint
		m_pStatusBar->UpdateWindow();

		//	Tell ncapi people window is changing position. //JOKI
		if( m_pStatusBar->m_pProxy2Frame) {
            CFrameGlue *pFrame = m_pStatusBar->m_pProxy2Frame;
		    if(pFrame->GetMainContext() && pFrame->GetMainContext()->GetContext()->type == MWContextBrowser){
 			    CStatusBarChangeItem::Changing(pFrame->GetMainContext()->GetContextID(), lpszText);
		    }
		}

	}
}

const char *CGenericStatusBar::GetStatusText()
{
	return m_csStatus;
}

void CGenericStatusBar::SetProgress(int iProg)
{
	m_iProg = iProg < -1 ? 0 : (iProg > 100 ? 100 : iProg);

	if ( m_bModal ) {
		if ( m_pModalStatus ) {
			m_pModalStatus->SetPercentDone(m_iProg);
		}
	}
	if ( m_pStatusBar ) {
		m_pStatusBar->SetPercentDone(m_iProg);	
	}
}

int CGenericStatusBar::GetProgress()
{
	return m_iProg;
}

void CGenericStatusBar::ProgressComplete()
{
	ModalStatus( FALSE, 0, NULL );
}

HWND CGenericStatusBar::GetHWnd()
{
	if (m_pStatusBar)
		return m_pStatusBar->m_hWnd;

	return NULL;
}

void CGenericStatusBar::StartAnimation()
{
    //
    // It uses this notification as a cue to be in "Cylon" mode when progress is unspecified.
    //
    if( m_pStatusBar )    
    {
       m_pStatusBar->StartAnimation();
    }
}

void CGenericStatusBar::StopAnimation()
{
    //
    // It uses this notification as a cue to end "Cylon" mode.
    //
    if( m_pStatusBar )    
    {
       m_pStatusBar->StopAnimation();
    }
}
/////////////////////////////////////////////////////////////////////////////
// CStatusBarChangeItem

//Begin JOKI
CPtrList CStatusBarChangeRegistry::m_Registry;

void CStatusBarChangeItem::Changing(DWORD dwWindowID, LPCSTR lpStatusMsg)	
{
	POSITION rIndex = m_Registry.GetHeadPosition();
	CStatusBarChangeItem *pChange;
	while(rIndex != NULL)	{
		pChange = (CStatusBarChangeItem *)m_Registry.GetNext(rIndex);
		if(pChange->GetWindowID() == dwWindowID)	{
			pChange->StatusChanging(lpStatusMsg);
		}
	}
}

void CDDEStatusBarChangeItem::StatusChanging(LPCSTR lpStatusMsg)	
{
	//TRACE("----------------"); TRACE(lpStatusMsg); TRACE("\r\n");

	//	Just call the DDE implementation, it will handle the details.
	if(*lpStatusMsg && *lpStatusMsg != ' ' && !IsSameAsLastMsgSent(lpStatusMsg))
		CDDEWrapper::StatusBarChange(this, lpStatusMsg);
}

BOOL CDDEStatusBarChangeItem::IsSameAsLastMsgSent(LPCSTR lpCurMsg)
{

	if(m_csLastMsgSent.CompareNoCase(lpCurMsg) == 0)
		return TRUE;
	else
	{
		m_csLastMsgSent = lpCurMsg;
		return FALSE;
	}
}

BOOL CDDEStatusBarChangeItem::DDERegister(CString& csServiceName, DWORD dwWindowID)	
{
	//	Purpose:	Register a server to monitor when a status bar msg changes

    //  Don't allow the mimimized window to be monitored.
    if(dwWindowID == 0) {
        return(FALSE);
    }

	//	Make sure such a window exists.
	CAbstractCX *pCX = CAbstractCX::FindContextByID(dwWindowID);
	if(NULL == pCX)	{
		return(FALSE);
	}
	if(pCX->GetContext()->type != MWContextBrowser)	{
		return(FALSE);
	}

	//	Looks like it will work.
	CDDEStatusBarChangeItem *pDontCare = new CDDEStatusBarChangeItem(csServiceName, dwWindowID);
	return(TRUE);
}

BOOL CDDEStatusBarChangeItem::DDEUnRegister(CString& csServiceName, DWORD dwWindowID)	
{
	//	Purpose:	UnRegister a server to monitor when a status bar msg changes

	//	Allright, we need to go through ever entry in our registry.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CStatusBarChangeItem *pCmp;
	CDDEStatusBarChangeItem *pDelme = NULL;
	while(rIndex != NULL)	{
		pCmp = (CStatusBarChangeItem *)m_Registry.GetNext(rIndex);
		pDelme = (CDDEStatusBarChangeItem *)pCmp;
		
		if(pDelme->GetServiceName() == csServiceName && pDelme->GetWindowID() == dwWindowID)	{
			break;
		}
		pDelme = NULL;
	}
	
    if(pDelme == NULL)	{
    	return(FALSE);
    }
    else	{
    	delete(pDelme);
    	return(TRUE);
    }
}
//End JOKI

/////////////////////////////////////////////////////////////////////
// CGenericChrome

STDMETHODIMP CGenericChrome::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
    if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IChrome))
   		*ppv = (LPCHROME) this;
	else if (IsEqualIID(refiid, IID_INSToolBar))
		*ppv = (LPNSTOOLBAR) m_pToolBar;
	else if (IsEqualIID(refiid, IID_INSAnimation))
		*ppv = (LPNSANIMATION) m_pToolBar;
	else if (IsEqualIID(refiid, IID_INSStatusBar))
		*ppv = (LPNSSTATUSBAR) m_pStatusBar;
	else if (m_pOuterUnk)
		return m_pOuterUnk->QueryInterface(refiid, ppv);

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CGenericChrome::AddRef(void)
{
	if (m_pOuterUnk)
		m_pOuterUnk->AddRef();
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CGenericChrome::Release(void)
{
	ULONG ulRef;
// Can't do this yet since contexts are not true
// COM objectects.
//	if (m_pOuterUnk)
//		m_pOuterUnk->Release();

	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) {
		delete this;   	
	}
	return ulRef;   	
}
// Menu bar stuff

void CGenericChrome::SetMenu( UINT nIDResource )
{
	HMENU hMenuOld = ::GetMenu(m_pParent->m_hWnd);
	HMENU hMenuNew = ::LoadMenu( AfxGetResourceHandle(), 
								 MAKEINTRESOURCE(nIDResource) );
	if ( hMenuNew ) {
		m_pParent->m_hMenuDefault = hMenuNew;
		if ( hMenuOld ) {
			::DestroyMenu( hMenuOld );
		}
		::SetMenu(m_pParent->m_hWnd, hMenuNew);
		::DrawMenuBar(m_pParent->m_hWnd);
	}

	HACCEL hAccelOld = m_pParent->m_hAccelTable;
	HACCEL hAccelNew = ::LoadAccelerators( AfxGetResourceHandle(), 
										   MAKEINTRESOURCE(nIDResource) );
	if( hAccelNew ) {
		if ( hAccelOld ) {
			::FreeResource((HGLOBAL) hAccelOld);
		}
		m_pParent->m_hAccelTable = hAccelNew;
	}

}

//#ifndef	NO_TAB_NAVIGATION 
BOOL CGenericChrome::procTabNavigation( UINT nChar, UINT firstTime, UINT controlKey )
{
	// spaceBar will be received by child who has focus.
	if( nChar != VK_TAB )
		return( FALSE );

	
	if( ( firstTime || m_tabFocusInChrom == TAB_FOCUS_IN_NULL)
		&& GetToolbarVisible(ID_LOCATION_TOOLBAR) ) {
		// location bar is the first child to get the focus.
		SetToolbarFocus(ID_LOCATION_TOOLBAR);
		m_tabFocusInChrom = TAB_FOCUS_IN_LOCATION_BAR ;
		return TRUE;
	}

	// I have nothing to tab to.
	m_tabFocusInChrom = TAB_FOCUS_IN_NULL;
	return( FALSE );
}
//#endif	/* NO_TAB_NAVIGATION */

// General Toolbar functionality
void CGenericChrome::ShowToolbar(UINT nToolbarID, BOOL bShow)
{
	if(m_pCustToolbar)
		m_pCustToolbar->ShowToolbar(nToolbarID, bShow);

	if (nToolbarID == ID_PERSONAL_TOOLBAR) // Hack. Show Aurora if and only if a personal toolbar is shown.
	{
		if (m_pParent->IsKindOf(RUNTIME_CLASS(CGenericFrame)))
		{
			CGenericFrame* genFrame = (CGenericFrame*)m_pParent;
			
			// Create NavCenter unless bShow is FALSE... then we hide it.  HACK!
			// THIS CODE WILL BE REMOVED! JUST TEMPORARILY HACKED TO PREVENT NAVCENTER
			// FROM SHOWING UP EVERYWHERE!
			if (!theApp.m_bInGetCriticalFiles && genFrame->AllowDocking() && 
				!theApp.m_ParentAppWindow && !theApp.m_bKioskMode)
			{
				// Show the selector if the pref says we should.
				BOOL bSelVisible;
				PREF_GetBoolPref(gPrefSelectorVisible, &bSelVisible);
				if (bSelVisible && bShow)
					theApp.CreateNewNavCenter(genFrame);
			}

			CNSNavFrame* navFrame = genFrame->GetDockedNavCenter();
			if (navFrame && !bShow)
			{
				// Destroy the Nav Center.
				navFrame->DeleteNavCenter();
			}
		}
	}
}

BOOL CGenericChrome::GetToolbarVisible(UINT nToolbarID)
{
	if(m_pCustToolbar)
		return m_pCustToolbar->IsWindowShowing(nToolbarID);
	else
		return FALSE;
}

CWnd *CGenericChrome::GetToolbar(UINT nToolbarID)
{
	if(m_pCustToolbar)
		return m_pCustToolbar->GetToolbar(nToolbarID);
	else
		return NULL;
}

void CGenericChrome::SetToolbarFocus(UINT nToolbarID)
{
	if(m_pCustToolbar)
	{
		CWnd *pToolbar = m_pCustToolbar->GetToolbar(nToolbarID);
		if(pToolbar && pToolbar->IsWindowVisible())
			pToolbar->SetFocus();
	}
}

// nPos, bOpen, and bShowing are IN/OUT parameters. Values going in are default values and values
// coming out are the values from the registry or default if not in the registry.  Or if there's
// another window open of the same type, the values are taken from that window.
void CGenericChrome::LoadToolbarConfiguration(UINT nToolbarID, CString &csToolbarName, int32 & nPos, BOOL & bOpen, BOOL & bShowing)
{

	if(m_pCustToolbar)
	{
		CFrameWnd *pFrame = FEU_GetLastActiveFrameByCustToolbarType(m_toolbarName, m_pParent, TRUE);

		// if there's another window of this type, copy its settings
		if(pFrame && pFrame != m_pParent && pFrame->IsKindOf(RUNTIME_CLASS(CGenericFrame)))
		{
			CGenericFrame *pGenFrame = (CGenericFrame *)pFrame;
	
			CWnd *pToolbar = pGenFrame->GetChrome()->GetCustomizableToolbar()->GetToolbar(nToolbarID);

			if(pToolbar)
			{
				nPos = pGenFrame->GetChrome()->GetCustomizableToolbar()->GetWindowPosition(pToolbar);
				bShowing = pGenFrame->GetChrome()->GetCustomizableToolbar()->IsWindowShowing(pToolbar);
				bOpen = !pGenFrame->GetChrome()->GetCustomizableToolbar()->IsWindowIconized(pToolbar);
			}
		}
		// otherwise use the settings from the registry.
		else
		{
			CString prefName = CString("custtoolbar.") + m_toolbarName;

			prefName = prefName + ".";
			prefName = prefName + csToolbarName;

			int nSize = 100;
			LPTSTR pBuffer = prefName.GetBuffer(nSize);
			FEU_ReplaceChar(pBuffer, ' ', '_');
			prefName.ReleaseBuffer();

			PREF_GetBoolPref(prefName + ".open", &bOpen);
			PREF_GetIntPref(prefName + ".position", &nPos);
			PREF_GetBoolPref(prefName + ".showing", &bShowing);
		}
	}


}

void CGenericChrome::SaveToolbarConfiguration(UINT nToolbarID, CString &csToolbarName)
{
	if(m_pCustToolbar)
	{
		// if we aren't supposed to save, then return
		if(!m_pCustToolbar->GetSaveToolbarInfo()) return;

		CString prefName = CString("custtoolbar.") + m_toolbarName;
		CWnd *pToolbar = m_pCustToolbar->GetToolbar(nToolbarID);
		
		if(pToolbar != NULL)
		{
			int nPos = m_pCustToolbar->GetWindowPosition(pToolbar);
			BOOL bShowing = m_pCustToolbar->IsWindowShowing(pToolbar);
			BOOL bOpen = !m_pCustToolbar->IsWindowIconized(pToolbar);

			prefName = prefName + ".";
			prefName = prefName + csToolbarName;

			int nSize = 100;
			LPTSTR pBuffer = prefName.GetBuffer(nSize);
			FEU_ReplaceChar(pBuffer, ' ', '_');
			prefName.ReleaseBuffer();

			PREF_SetIntPref(prefName + ".position", nPos);
			PREF_SetBoolPref(prefName + ".showing", bShowing);
			PREF_SetBoolPref(prefName + ".open", bOpen);
		}
	}
}

void CGenericChrome::SetSaveToolbarInfo(BOOL bSaveToolbarInfo)
{
	if(m_pCustToolbar)
		m_pCustToolbar->SetSaveToolbarInfo(bSaveToolbarInfo);

}

// Animation Stuff
void CGenericChrome::StartAnimation()
{
	LPNSANIMATION pIAnimation = NULL;
	QueryInterface( IID_INSAnimation, (LPVOID *) &pIAnimation );
	if (pIAnimation) {
		pIAnimation->StartAnimation();
		pIAnimation->Release();
	}
	LPNSSTATUSBAR pIStatusBar = NULL;
	QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->StartAnimation();
		pIStatusBar->Release();
	}
	if(m_pCustToolbar)
	{
	//	CCommandToolbar * pCommandToolbar =(CCommandToolbar*)GetToolbar(ID_NAVIGATION_TOOLBAR);

	//	if(pCommandToolbar)
	//		pCommandToolbar->ReplaceButton(ID_NAVIGATE_RELOAD, ID_NAVIGATE_INTERRUPT);
		m_pCustToolbar->StartAnimation();
	}
}

void CGenericChrome::StopAnimation()
{
	LPNSANIMATION pIAnimation = NULL;
	QueryInterface( IID_INSAnimation, (LPVOID *) &pIAnimation );
	if (pIAnimation) {
		pIAnimation->StopAnimation();
		pIAnimation->Release();
	} 

	LPNSSTATUSBAR pIStatusBar = NULL;
	QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->ProgressComplete();
		pIStatusBar->StopAnimation();        
		pIStatusBar->Release();
	}

	if(m_pCustToolbar)
	{
	//	CCommandToolbar * pCommandToolbar =(CCommandToolbar*)GetToolbar(ID_NAVIGATION_TOOLBAR);

	//	if(pCommandToolbar)
	//		pCommandToolbar->ReplaceButton(ID_NAVIGATE_INTERRUPT, ID_NAVIGATE_RELOAD);
		m_pCustToolbar->StopAnimation();
	}
}

int CGenericChrome::CreateCustomizableToolbar(CString toolbarName, int nMaxToolbars, BOOL bHasAnimation)
{
	m_pCustToolbar=new CCustToolbar(nMaxToolbars);

	if(! m_pCustToolbar->Create(m_pParent, bHasAnimation))
		return FALSE;

	m_pParent->RecalcLayout();
	m_toolbarName = toolbarName;
	return TRUE;

}

int CGenericChrome::CreateCustomizableToolbar(UINT nStringID, int nMaxToolbars, BOOL bHasAnimation)
{
	CString str;

	str.LoadString(nStringID);

	return CreateCustomizableToolbar(str, nMaxToolbars, bHasAnimation);

}

CString CGenericChrome::GetCustToolbarString()
{
	return m_toolbarName;
}

void CGenericChrome::RenameCustomizableToolbar(UINT nStringID)
{
	m_toolbarName.LoadString(nStringID);
}

void CGenericChrome::FinishedAddingBrowserToolbars(void)
{
	m_pCustToolbar->FinishedAddingNewWindows();

}

void CGenericChrome::SetToolbarStyle( int nToolbarStyle )
{
	if(m_pCustToolbar != NULL)
	{
		m_pCustToolbar->SetToolbarStyle(nToolbarStyle);
	}
}

BOOL CGenericChrome::CustToolbarShowing(void)
{
	return m_pCustToolbar->IsWindowVisible();
}

void CGenericChrome::ViewCustToolbar(BOOL bShow)
{
	if( m_pCustToolbar ){
        if(bShow)
		    m_pCustToolbar->ShowWindow(SW_SHOWNA);
	    else
		    m_pCustToolbar->ShowWindow(SW_HIDE);

        m_pCustToolbar->GetParentFrame()->RecalcLayout();
    }
}

void CGenericChrome::Customize(void)
{

}


CCustToolbar * CGenericChrome::GetCustomizableToolbar(void)
{
	return m_pCustToolbar;
}


void CGenericChrome::ImagesButton(BOOL bShowImagesButton)
{

	CCommandToolbar * pCommandToolbar =(CCommandToolbar*)GetToolbar(ID_NAVIGATION_TOOLBAR);

	if(pCommandToolbar)
	{
		if(bShowImagesButton)
		{
			pCommandToolbar->ShowButtonByCommand(ID_VIEW_LOADIMAGES, 5);
		}
		else
		{
			pCommandToolbar->HideButtonByCommand(ID_VIEW_LOADIMAGES);
		}
	}
}

//	Window Title Stuff
void CGenericChrome::SetWindowTitle(const char *lpszText)
{
	m_csWindowTitle = lpszText;

	CString cs;
	cs = m_csDocTitle;
	if (!m_csDocTitle.IsEmpty()) {
		cs += " - ";
	}
	cs += m_csWindowTitle;
	m_pParent->SetWindowText(cs);
}

void CGenericChrome::SetDocumentTitle(const char *lpszText)
{
	m_csDocTitle = lpszText;

	CString cs;
	cs = m_csDocTitle;
	if (!m_csDocTitle.IsEmpty()) {
		cs += " - ";
	}
	cs += m_csWindowTitle;
	m_pParent->SetWindowText(cs);
}	

// Constructor and Destructor
CGenericChrome::CGenericChrome( LPUNKNOWN pOuterUnk )
{
	// XXX WHS Remove this when you make sure that
	// SetWindowTitle is called in all the right
	// places.
	m_csWindowTitle = XP_AppName;

	m_bHasStatus = FALSE;
	m_pParent = NULL;
	m_pOuterUnk = pOuterUnk;
	m_ulRefCount = 0;
	m_pToolBar = new CGenericToolBar( (LPUNKNOWN ) this );
	m_pStatusBar = new CGenericStatusBar( (LPUNKNOWN) this );
	m_pCustToolbar = NULL;

//#ifndef 	NO_TAB_NAVIGATION
	m_tabFocusInChrom = TAB_FOCUS_IN_NULL;		// I don't have tab focus
//#endif	/* NO_TAB_NAVIGATION */

}

CGenericChrome::~CGenericChrome()
{
	ApiApiPtr(api);
	api->RemoveInstance(this);

	if(m_pCustToolbar != NULL)
		delete m_pCustToolbar;

	delete m_pToolBar;
    
    delete m_pStatusBar;
}

void CGenericChrome::Initialize(CFrameWnd * pParent) 
{
	ASSERT(pParent);
	m_pParent = pParent;
}

//
// Class Factory

class CGenericChromeFactory :	public CGenericFactory
{
public:
	CGenericChromeFactory();
	~CGenericChromeFactory();
	// IClassFactory Interface
	STDMETHODIMP			CreateInstance(LPUNKNOWN,REFIID,LPVOID*);
};

STDMETHODIMP CGenericChromeFactory::CreateInstance(
	LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj)
{
	*ppvObj = NULL;
	CGenericChrome * pChrome = new CGenericChrome( pUnkOuter );
	return pChrome->QueryInterface(refiid,ppvObj);   
}


CGenericChromeFactory::CGenericChromeFactory()
{
	ApiApiPtr(api);
	api->RegisterClassFactory(APICLASS_CHROME,(LPCLASSFACTORY)this);
}

CGenericChromeFactory::~CGenericChromeFactory()
{
}

DECLARE_FACTORY(CGenericChromeFactory);
