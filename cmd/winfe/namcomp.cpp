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
// addrdlg.cpp : implementation file
//

#include "stdafx.h"
#include "wfemsg.h"
#include "msg_srch.h"
#include "dirprefs.h"
#include "namcomp.h"
#include "apiaddr.h"
#include "nethelp.h" 
#include "prefapi.h"
#include "intl_csi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
#include "xpgetstr.h"
};


class CNameCompletion;

CNameCompletionCX::CNameCompletionCX(CNameCompletion *pDialog)  
: CStubsCX(AddressCX, MWContextAddressBook)
{
	m_pDialog = pDialog;
	m_lPercent = 0;
	m_bAnimated = FALSE;
}


void CNameCompletionCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent ) {
	//	Ensure the safety of the value.

	lPercent = lPercent < 0 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_lPercent == lPercent ) {
		return;
	}

	m_lPercent = lPercent;
	if (m_pDialog) {
		m_pDialog->SetProgressBarPercent(lPercent);
	}
}

void CNameCompletionCX::Progress(MWContext *pContext, const char *pMessage) {
	if ( m_pDialog ) {
		m_pDialog->SetStatusText(pMessage);
	}
}

int32 CNameCompletionCX::QueryProgressPercent()	{
	return m_lPercent;
}


void CNameCompletionCX::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CStubsCX::AllConnectionsComplete(pContext);

	//	Also, we can clear the progress bar now.
	m_lPercent = 0;
	if ( m_pDialog ) {
		m_pDialog->SetProgressBarPercent(m_lPercent);
		m_pDialog->AllConnectionsComplete(pContext);
	}
    if (m_pDialog) {
		m_pDialog->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

void CNameCompletionCX::UpdateStopState( MWContext *pContext )
{
    if (m_pDialog) {
		m_pDialog->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

CWnd *CNameCompletionCX::GetDialogOwner() const {
	return m_pDialog;
}


/////////////////////////////////////////////////////////////////////////////
// CNameCompletionEntryList

STDMETHODIMP CNameCompletionEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) m_pNameCompletion;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CNameCompletionEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CNameCompletionEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CNameCompletionEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pNameCompletion) {
		m_pNameCompletion->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CNameCompletionEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pNameCompletion) {
		m_pNameCompletion->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CNameCompletionEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CNameCompletionEntryList::SelectItem( MSG_Pane* pane, int item )
{
}


/////////////////////////////////////////////////////////////////////////////
// CNameCompletion

CNameCompletion::CNameCompletion(LPCTSTR lpszSearchString,
	CWnd * parent): CDialog(CNameCompletion::IDD, parent)
{
	int result = 0;
	INTL_CharSetInfo csi;

	m_pCX = new CNameCompletionCX( this );
	csi = LO_GetDocumentCharacterSetInfo(m_pCX->GetContext());

	m_pCX->GetContext()->type = MWContextAddressBook;
	m_pCX->GetContext()->fancyFTP = TRUE;
	m_pCX->GetContext()->fancyNews = TRUE;
	m_pCX->GetContext()->intrupt = FALSE;
	m_pCX->GetContext()->reSize = FALSE;
	INTL_SetCSIWinCSID(csi, CIntlWin::GetSystemLocaleCsid());

	m_addrBookPane = NULL;
	m_pOutliner = NULL;
	m_pOutlinerParent = NULL;
	m_bSearching = FALSE;
	m_lpszSearchString = lpszSearchString;

	CNameCompletionEntryList *pInstance = new CNameCompletionEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );

	HandleErrorReturn((result = AB_CreateAddressBookPane(&m_addrBookPane,
			m_pCX->GetContext(),
			WFE_MSGGetMaster())));

	//{{AFX_DATA_INIT(CNameCompletion)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CNameCompletion::CleanupOnClose()
{
	// DestroyContext will call Interrupt, but if we wait until after DestroyContext
	// to call MSG_SearchFree, the MWContext will be gone, and we'll be reading freed memory
	if (XP_IsContextBusy (m_pCX->GetContext()))
		XP_InterruptContext (m_pCX->GetContext());
	MSG_SearchFree ((MSG_Pane*) m_addrBookPane);

	if (m_pIAddrList)
		m_pIAddrList->Release();

	if (m_addrBookPane)
		HandleErrorReturn(AB_CloseAddressBookPane(&m_addrBookPane));

	if(!m_pCX->IsDestroyed()) {
		m_pCX->DestroyContext();
	}

	if (m_pFont){
		theApp.ReleaseAppFont(m_pFont);
	}

	if (m_pOutlinerParent){
		delete m_pOutlinerParent;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CNameCompletion Overloaded methods
/////////////////////////////////////////////////////////////////////////////
BOOL CNameCompletion::OnInitDialog( )
{
	if (CDialog::OnInitDialog()) {

		CWnd* widget;
		CRect rect2, rect3, rect4;
		UINT aIDArray[] = { IDS_SECURITY_STATUS, IDS_TRANSFER_STATUS, ID_SEPARATOR};
		int result = 0;

		DIR_Server* dir = (DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, 1);

		XP_ASSERT (dir);

		if (!dir)
			return FALSE;

		HandleErrorReturn((result = AB_InitializeAddressBookPane(m_addrBookPane,
			dir,
			theApp.m_pABook,
			ABFullName,
			TRUE)));

		if (result) {
			EndDialog(IDCANCEL);
			return TRUE;
		}

		// create the outliner
		widget = GetDlgItem(IDC_ADDRESSLIST);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		GetClientRect(&rect4);
		ClientToScreen(&rect4);
		rect2.OffsetRect(-rect4.left, -rect4.top);

		widget->DestroyWindow ();

		// create the outliner control
		m_pOutlinerParent = new CNameCompletionOutlinerParent;
#ifdef _WIN32
		m_pOutlinerParent->CreateEx ( WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"), 
								WS_BORDER|WS_TABSTOP|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
								0, 0, 0, 0,
								this->m_hWnd, (HMENU) IDC_ADDRESSLIST);
#else
		rect3.SetRectEmpty();
		m_pOutlinerParent->Create( NULL, _T("NSOutlinerParent"), 
								  WS_BORDER|WS_TABSTOP|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								  rect3, this, IDC_ADDRESSLIST);
#endif

		m_pOutlinerParent->MoveWindow(&rect2, TRUE);
		m_pOutlinerParent->CreateColumns ( );
		m_pOutliner = (CNameCompletionOutliner *) m_pOutlinerParent->m_pOutliner;
		m_pOutliner->SetPane(m_addrBookPane);
		m_pOutliner->SetContext( m_pCX->GetContext() );

		// create the status bar
		widget = GetDlgItem(IDC_StatusRect);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		widget->DestroyWindow ();

		// create the status bar
		m_barStatus.Create(this, TRUE, FALSE);
		m_barStatus.MoveWindow(&rect2, TRUE);
		m_barStatus.SetIndicators( aIDArray, sizeof(aIDArray) / sizeof(UINT) );
		
		UpdateButtons();
	}

	return TRUE;
}


void CNameCompletion::Progress(const char *pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}


void CNameCompletion::SetProgressBarPercent(int32 lPercent)
{
	m_barStatus.SetPercentDone (lPercent);
} // END OF	FUNCTION CNameCompletion::DrawProgressBar()


/////////////////////////////////////////////////////////////////////////////
// CAddrDialog message handlers

BEGIN_MESSAGE_MAP(CNameCompletion, CDialog)
	//{{AFX_MSG_MAP(CNameCompletion)
	ON_BN_CLICKED( IDOK, OnOK)
	ON_BN_CLICKED( IDCANCEL, OnCancel)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CNameCompletion::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNameCompletion)
	//}}AFX_DATA_MAP
}


int CNameCompletion::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	int res = CDialog::OnCreate(lpCreateStruct);

	m_MailNewsResourceSwitcher.Reset();

	MSG_SetFEData( (MSG_Pane*) m_addrBookPane, (void *) m_pIAddrList );
	return res;
}


void CNameCompletion::OnOK() 
{
	CDialog::OnOK();
	CleanupOnClose();
}


void CNameCompletion::OnCancel() 
{
	CDialog::OnCancel();
	CleanupOnClose();
}


void CNameCompletion::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffStarting( asynchronous, notify,
												   where, num );
		}
	}
}

void CNameCompletion::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffFinishing( asynchronous, notify,
												    where, num );
			UpdateButtons();
		}
	}
}


void CNameCompletion::SetStatusText(const char* pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}


void CNameCompletion::SetSearchResults(MSG_ViewIndex index, int32 num)
{

	CString csStatus;

	ASSERT(m_pOutliner);
	AB_LDAPSearchResults(m_addrBookPane, index, num);
	if (num > 1 ) {
		csStatus.Format( szLoadString(IDS_SEARCHHITS), num );
	} else if ( num > 0 ) {
		csStatus.LoadString( IDS_SEARCHONEHIT );
	} else {
		csStatus.LoadString( IDS_SEARCHNOHITS );
	}

	m_barStatus.SetWindowText( csStatus );

}

STDMETHODIMP CNameCompletion::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) (LPMAILFRAME) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) this;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CNameCompletion::AddRef(void)
{
	return 0; // Not a real component
}

STDMETHODIMP_(ULONG) CNameCompletion::Release(void)
{
	return 0; // Not a real component
}

// IMailFrame interface
CMailNewsFrame *CNameCompletion::GetMailNewsFrame()
{
	return (CMailNewsFrame *) NULL; 
}

MSG_Pane *CNameCompletion::GetPane()
{
	return (MSG_Pane*) m_addrBookPane;
}

void CNameCompletion::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if (notify == MSG_PaneDirectoriesChanged) {
		if (IsSearching())
			PerformDirectorySearch ();
	}
}


void CNameCompletion::AllConnectionsComplete( MWContext *pContext )
{
	PerformDirectorySearch();

	int total = m_pOutliner->GetTotalLines();
	CString csStatus;
	if ( total > 1 ) {
		csStatus.Format( szLoadString( IDS_SEARCHHITS ), total );
	} else if ( total > 0 ) {
		csStatus.LoadString( IDS_SEARCHONEHIT );
	} else {
		csStatus.LoadString( IDS_SEARCHNOHITS );
	}
	m_barStatus.SetWindowText( csStatus );

	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
}


void CNameCompletion::OnComposeMsg()
{
	
}


void CNameCompletion::HandleErrorReturn(int errorid)
{	
	if (errorid) {
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			::MessageBox(NULL, XP_GetString(errorid), s, MB_OK);
	}
}


void CNameCompletion::PerformDirectorySearch ()
{
	CString cs;

	if ( m_bSearching) {
		// We've turned into stop button
		XP_InterruptContext( m_pCX->GetContext() );
		HandleErrorReturn(AB_FinishSearch(m_addrBookPane, m_pCX->GetContext()));
		m_bSearching = FALSE;
		return;
	}

	// Begin Search
	m_barStatus.SetWindowText( szLoadString( IDS_SEARCHING ) );
	m_bSearching = TRUE;
	m_pOutliner->UpdateCount();
	m_pOutliner->SetFocus();

	HandleErrorReturn(AB_SearchDirectory(m_addrBookPane, NULL));

}


//////////////////////////////////////////////////////////////////////////////
// CNameCompletionOutliner
BEGIN_MESSAGE_MAP(CNameCompletionOutliner, COutliner)
	//{{AFX_MSG_MAP(CNameCompletionOutliner)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CNameCompletionOutliner::CNameCompletionOutliner ( )
{
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_ADDRESSBOOK,16,16);
    }
	m_iMysticPlane = 0;
	m_hFont = NULL;
}

CNameCompletionOutliner::~CNameCompletionOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
}


void CNameCompletionOutliner::SetColumnsForDirectory (DIR_Server* pServer)
{
	int iCount = GetNumColumns();
	for (int i = 0; i < iCount; i++) {
		CString cs;
		int iColumn = GetColumnAtPos(i);
		if (pServer->dirType == LDAPDirectory)
		{
			DIR_AttributeId id;
			const char *text = NULL;
			switch (iColumn) {
			case ID_COLADDR_TYPE:
				text = NULL;
				break;
			case ID_COLADDR_NAME:
				MSG_SearchAttribToDirAttrib(attribCommonName, &id);		
				text = DIR_GetAttributeName(pServer, id);
				break;
			case ID_COLADDR_EMAIL:
				MSG_SearchAttribToDirAttrib(attrib822Address, &id);	
				text = DIR_GetAttributeName(pServer, id);
				break;
			case ID_COLADDR_COMPANY:
				MSG_SearchAttribToDirAttrib(attribOrganization, &id);	
				text = DIR_GetAttributeName(pServer, id);
				break;
			case ID_COLADDR_PHONE:
				MSG_SearchAttribToDirAttrib(attribPhoneNumber, &id);	
				text = DIR_GetAttributeName(pServer, id);
				break;
			case ID_COLADDR_LOCALITY:
				MSG_SearchAttribToDirAttrib(attribLocality, &id);	
				text = DIR_GetAttributeName(pServer, id);
				break;
			case ID_COLADDR_NICKNAME:
				text = NULL;	
				break;
			default:
				break;
			}
			if (text)
				SetColumnName(iColumn, text);
		}
		else
		{
			switch (iColumn) {
			case ID_COLADDR_TYPE:
				cs = "";
				break;
			case ID_COLADDR_NAME:
				cs.LoadString(IDS_USERNAME);		
				break;
			case ID_COLADDR_EMAIL:
				cs.LoadString(IDS_EMAILADDRESS);
				break;
			case ID_COLADDR_COMPANY:
				cs.LoadString(IDS_COMPANYNAME);
				break;
			case ID_COLADDR_PHONE:
				cs.LoadString(IDS_PHONE);
				break;
			case ID_COLADDR_LOCALITY:
				cs.LoadString(IDS_LOCALITY);
				break;
			case ID_COLADDR_NICKNAME:
				cs.LoadString(IDS_NICKNAME);
				break;
			default:
				break;
			}
		if (cs.GetLength())
			SetColumnName(iColumn, cs);
		}
	}
	GetParent()->Invalidate();
	GetParent()->UpdateWindow();
}


void CNameCompletionOutliner::UpdateCount( )
{
	uint32 count = 0;

	SetTotalLines(CASTINT(count));
}


void CNameCompletionOutliner::SetPane(ABPane *pane)
{
	m_pane = pane;
	uint32 count = 0;

	if (m_pane) {
		SetTotalLines(CASTINT(count));
		Invalidate();
		UpdateWindow();
	}
}

void CNameCompletionOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CNameCompletionOutliner::MysticStuffFinishing( XP_Bool asynchronous,
											 MSG_NOTIFY_CODE notify, 
											 MSG_ViewIndex where,
											 int32 num )
{

#ifdef _WIN32
			CWnd *pParent = GetParentOwner();
#else
			CWnd *pParent = GetOwner();      
			pParent = pParent->GetParent();
			ASSERT(pParent);
#endif

	switch ( notify ) 
	{

	case MSG_NotifyNone:
		break;

	case MSG_NotifyInsertOrDelete:
		// if its insert or delete then tell my frame to add the next chunk of values
		// from the search
		if (notify == MSG_NotifyInsertOrDelete 
			&& ((CNameCompletion*)pParent)->IsSearching() && num > 0) 
		{
			((CNameCompletion*)pParent)->SetSearchResults(where, num);
		}
		else
		{
		}
		break;

	case MSG_NotifyChanged:
		InvalidateLines( (int) where, (int) num );
		break;

	case MSG_NotifyAll:
	case MSG_NotifyScramble:
		Invalidate();
		break;
	}

	if (( !--m_iMysticPlane && m_pane)) 
	{
		uint32 count;
		SetTotalLines(CASTINT(count));
		Invalidate();
		UpdateWindow();
	}
}


void CNameCompletionOutliner::SetTotalLines( int count)
{
	COutliner::SetTotalLines(count);
}




BOOL CNameCompletionOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
	if ( iColumn != ID_COLADDR_TYPE )
        return COutliner::RenderData ( iColumn, rect, dc, text );

	int idxImage = 0;

    if (m_EntryLine.entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;

	m_pIUserImage->DrawImage ( idxImage,
		rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
	return TRUE;
}


int CNameCompletionOutliner::TranslateIcon ( void * pLineData )
{
	AB_EntryLine* line = (AB_EntryLine*) pLineData;
	int idxImage = 0;

    if (line->entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;
	return idxImage;
}

int CNameCompletionOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CNameCompletionOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

HFONT CNameCompletionOutliner::GetLineFont(void *pLineData)
{
	if (!m_hFont)
	{
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
		m_hFont = theApp.CreateAppFont( lf );
	    ::ReleaseDC(m_hWnd,hDC);
	}
	return m_hFont;
}


void * CNameCompletionOutliner::AcquireLineData ( int line )
{
	m_lineindex = line + 1;
	if ( line >= m_iTotalLines)
		return NULL;
	if (!AB_GetEntryLine(m_pane, line, &m_EntryLine ))
        return NULL;

	return &m_EntryLine;
}


void CNameCompletionOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}


void CNameCompletionOutliner::ReleaseLineData ( void * )
{
}


LPCTSTR CNameCompletionOutliner::GetColumnText ( UINT iColumn, void * pLineData )
{
	AB_EntryLine* line = (AB_EntryLine*) pLineData;

	switch (iColumn) {
		case ID_COLADDR_NAME:
			return line->fullname;
			break;
		case ID_COLADDR_EMAIL:
			return line->emailAddress;
			break;
		case ID_COLADDR_COMPANY:
			return line->companyName;
			break;
		case ID_COLADDR_PHONE:
			if (line->entryType == ABTypePerson)
				return line->workPhone;
			break;
		case ID_COLADDR_LOCALITY:
			return line->locality;
			break;
		case ID_COLADDR_NICKNAME:
			return line->nickname;
			break;
		default:
			break;
	}
    return ("");
}

void CNameCompletionOutliner::OnSelChanged()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif
	((CNameCompletion*) pParent)->UpdateButtons();
}

void CNameCompletionOutliner::OnSelDblClk()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CNameCompletionOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CNameCompletionOutlinerParent)
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CNameCompletionOutlinerParent::CNameCompletionOutlinerParent()
{

}


CNameCompletionOutlinerParent::~CNameCompletionOutlinerParent()
{
}


BOOL CNameCompletionOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CNameCompletionOutliner *pOutliner = (CNameCompletionOutliner *) m_pOutliner;
	
	// Calculate text offset from top using font height.
    TEXTMETRIC tm;
    dc.GetTextMetrics ( &tm );
    cy = ( rect.bottom - rect.top - tm.tmHeight ) / 2;
        
	// Draw Text String
	dc.TextOut (rect.left + cx, rect.top + cy, text, _tcslen(text) );

    return TRUE;
}


COutliner * CNameCompletionOutlinerParent::GetOutliner ( void )
{
    return new CNameCompletionOutliner;
}


void CNameCompletionOutlinerParent::CreateColumns ( void )
{ 
	CString cs; 

	m_pOutliner->AddColumn ("",		ID_COLADDR_TYPE,			24, 0, ColumnFixed, 0, TRUE );
	cs.LoadString(IDS_USERNAME);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_NAME,		175, 0, ColumnVariable, 1500);
	cs.LoadString(IDS_EMAILADDRESS);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_EMAIL,		175, 0, ColumnVariable, 1500); 
	cs.LoadString(IDS_COMPANYNAME);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_COMPANY,		175, 0, ColumnVariable, 1500 ); 
	cs.LoadString(IDS_PHONE);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_PHONE,		175, 0, ColumnVariable, 1500, FALSE);
	cs.LoadString(IDS_LOCALITY);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_LOCALITY,	175, 0, ColumnVariable, 1500 );
	cs.LoadString(IDS_NICKNAME);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_NICKNAME,	175, 0, ColumnVariable, 1500 );
	m_pOutliner->SetHasPipes( FALSE );

	m_pOutliner->SetVisibleColumns(DEF_VISIBLE_COLUMNS);
	m_pOutliner->LoadXPPrefs("mailnews.abook_columns_win");

}


BOOL CNameCompletionOutlinerParent::ColumnCommand ( int idColumn )
{	
	ABID lastSelection;

	CNameCompletionOutliner *pOutliner = (CNameCompletionOutliner *) m_pOutliner;
	
	if (pOutliner->GetFocusLine() != -1)
		lastSelection = AB_GetEntryIDAt((AddressPane*) pOutliner->GetPane(), pOutliner->GetFocusLine());

	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );

	switch (idColumn) {
		case ID_COLADDR_TYPE:
			AB_Command(pOutliner->GetPane(), AB_SortByTypeCmd, 0, 0);
			break;
		case ID_COLADDR_NAME:
			AB_Command(pOutliner->GetPane(), AB_SortByFullNameCmd, 0, 0);
			break;
		case ID_COLADDR_NICKNAME:
			AB_Command(pOutliner->GetPane(), AB_SortByNickname, 0, 0);
			break;
		case ID_COLADDR_LOCALITY:
			AB_Command(pOutliner->GetPane(), AB_SortByLocality, 0, 0);
			break;
		case ID_COLADDR_COMPANY:
			AB_Command(pOutliner->GetPane(), AB_SortByCompanyName, 0, 0);
			break;
		case ID_COLADDR_EMAIL:
			AB_Command(pOutliner->GetPane(), AB_SortByEmailAddress, 0, 0);
			break;
		default:
			AB_Command(pOutliner->GetPane(), AB_SortByFullNameCmd, 0, 0);
			break;
	}
	

	if (pOutliner->GetFocusLine() != -1) {
		uint index = CASTUINT(AB_GetIndexOfEntryID ((AddressPane*) pOutliner->GetPane(), lastSelection));
		pOutliner->SelectItem (index);
		pOutliner->ScrollIntoView(index);
	}

	Invalidate();
	pOutliner->Invalidate();
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
	return TRUE;
}

void CNameCompletionOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.autocomp_columns_win");
}


void CNameCompletion::OnHelp() 
{
	NetHelp(HELP_SELECT_ADDRESSES);
}


void CNameCompletion::UpdateButtons()
{
	if (m_pOutliner)
	{
		// need to determine when to actually enable this
		BOOL bEnable = TRUE;
		GetDlgItem(IDOK)->EnableWindow(bEnable);
	}
}

