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

// namcomp : implementation file
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
#include "abcom.h"
#include "addrfrm.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
#include "xpgetstr.h"
};

#define BUTTON_SPACING 5
#define PICKER_RIGHT_MARGIN 20
#define PICKER_LEFT_MARGIN 20
#define PICKER_TOP_MARGIN 20
#define PICKER_BOTTOM_MARGIN 20
#define LIST_BUTTON_MARGIN 10
#define BUTTON_STATUS_MARGIN 10

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

	m_pPickerPane = NULL;
	m_pOutliner = NULL;
	m_pOutlinerParent = NULL;
	m_bSearching = FALSE;
	m_lpszSearchString = lpszSearchString;
	m_bInitDialog = FALSE;
	m_pCookie = NULL;
	m_bFreeCookie = TRUE;
	CNameCompletionEntryList *pInstance = new CNameCompletionEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );

	HandleErrorReturn((result = AB_CreateABPickerPane(&m_pPickerPane,
		m_pCX->GetContext(), WFE_MSGGetMaster(), 20)));

#ifdef MOZ_NEWADDR
	HandleErrorReturn(AB_SetShowPropertySheetForEntryFunc(m_pPickerPane, ShowPropertySheetForEntry));
#endif
	//{{AFX_DATA_INIT(CNameCompletion)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CNameCompletion::~CNameCompletion()
{
	if(m_pCookie && m_bFreeCookie)
	{
		AB_FreeNameCompletionCookie(m_pCookie);
	}

}

void CNameCompletion::CleanupOnClose()
{
	// DestroyContext will call Interrupt, but if we wait until after DestroyContext
	// to call MSG_SearchFree, the MWContext will be gone, and we'll be reading freed memory
	if (XP_IsContextBusy (m_pCX->GetContext()))
		XP_InterruptContext (m_pCX->GetContext());
	MSG_SearchFree ((MSG_Pane*) m_pPickerPane);


	if (m_pPickerPane)
		HandleErrorReturn(AB_ClosePane(m_pPickerPane));

	if (m_pOutlinerParent){
		delete m_pOutlinerParent;
	}

	if (m_pIAddrList)
		m_pIAddrList->Release();

	if(!m_pCX->IsDestroyed()) {
		m_pCX->DestroyContext();
	}

	if (m_pFont){
		theApp.ReleaseAppFont(m_pFont);
	}

}

/////////////////////////////////////////////////////////////////////////////
// CNameCompletion Overloaded methods
/////////////////////////////////////////////////////////////////////////////

BOOL CNameCompletion::OnCommand( WPARAM wParam, LPARAM lParam )
{
	UINT nID = wParam;

	if ( nID >= FIRST_ADDSENDER_MENU_ID && nID <= LAST_ADDSENDER_MENU_ID ) {
		OnAddToAddressBook( nID );
		return TRUE;
	}

	return CDialog::OnCommand( wParam, lParam );
}

BOOL CNameCompletion::OnInitDialog( )
{
	if (CDialog::OnInitDialog()) {

		CWnd* widget;
		CRect rect2, rect3, rect4;
		UINT aIDArray[] = { IDS_TRANSFER_STATUS, ID_SEPARATOR, IDS_ONLINE_STATUS};
		int result = 0;


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

		m_pOutliner = (CNameCompletionOutliner *) m_pOutlinerParent->m_pOutliner;
		m_pOutliner->SetPane(m_pPickerPane);
		m_pOutliner->SetContext( m_pCX->GetContext() );
		m_pOutliner->SetMultipleSelection(FALSE);

		m_pOutlinerParent->MoveWindow(&rect2, TRUE);
		m_pOutlinerParent->CreateColumns ( );
		m_pOutlinerParent->EnableFocusFrame();

		// create the status bar
		widget = GetDlgItem(IDC_StatusRect);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		widget->DestroyWindow ();

		// create the status bar
		m_barStatus.Create(this, FALSE, FALSE);
		m_barStatus.MoveWindow(&rect2, TRUE);
		m_barStatus.SetIndicators( aIDArray, sizeof(aIDArray) / sizeof(UINT) );
		
		//set initial online status
		int idx = m_barStatus.CommandToIndex(IDS_ONLINE_STATUS);
		if (idx > -1)
		{
			UINT nID = IDS_ONLINE_STATUS;
			UINT nStyle; 
			int nWidth;
			m_barStatus.GetPaneInfo( idx, nID, nStyle, nWidth );
			if (!NET_IsOffline())
				m_barStatus.SetPaneInfo(idx, IDS_ONLINE_STATUS, SBPS_NORMAL, nWidth);
			else
				m_barStatus.SetPaneInfo(idx, IDS_ONLINE_STATUS, SBPS_DISABLED, nWidth);
		}

		UpdateButtons();

		// add the correct title.
		CString itemsMatching;
		itemsMatching.LoadString(IDS_ITEMSMATCHING);
		
		CString title;
		title.Format("%s \" %s \"", itemsMatching, m_lpszSearchString);
		SetWindowText(title);

	}
	m_bInitDialog = TRUE;

	//resize so everything gets laid out correct after m_bInitDialog set
	CRect windowRect;
	GetWindowRect(windowRect);
	SetWindowPos(NULL, 0, 0, windowRect.Width() + 1, windowRect.Height() + 1, SWP_NOMOVE);

	//get rid of current cookie
	if(m_pCookie && m_bFreeCookie)
	{
		AB_FreeNameCompletionCookie(m_pCookie);
	}
	m_pCookie = NULL;
	m_bFreeCookie = TRUE;

	//start the name completion search
#ifdef FE_IMPLEMENTS_VISIBLE_NC
	int result = AB_NameCompletionSearch(m_pPickerPane, m_lpszSearchString, NULL, TRUE, NULL);
#else
	int result = AB_NameCompletionSearch(m_pPickerPane, m_lpszSearchString, NULL, NULL);
#endif
	return TRUE;
}

int CNameCompletion::DoModal ()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;
	return CDialog::DoModal();
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
	ON_WM_SIZE()
	ON_COMMAND(ID_HELP, OnHelp)
	ON_UPDATE_COMMAND_UI(ID_ITEM_PROPERTIES, OnUpdateProperties)
	ON_COMMAND(ID_ITEM_PROPERTIES, OnProperties)
	ON_UPDATE_COMMAND_UI(IDS_ONLINE_STATUS, OnUpdateOnlineStatus)
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

	if(m_pPickerPane)
	{
		MSG_SetFEData( (MSG_Pane*) m_pPickerPane, (void *) m_pIAddrList );
	}
	return res;
}


void CNameCompletion::OnSize( UINT nType, int cx, int cy )
{
	CDialog::OnSize(nType, cx, cy);

	//if init dialog has been called then we can resize widgets inside.
	if(m_bInitDialog)
	{
		CWnd *ok;
		CWnd *cancel;
		CWnd *statusSeparator;
		int y = cy;
		int x;

		CRect statusRect;
		CRect okRect;
		CRect cancelRect;
		CRect statusSeparatorRect;

		m_barStatus.GetWindowRect(statusRect);
		m_barStatus.MoveWindow(0, y - statusRect.Height(),
							   cx, statusRect.Height());

		y -= statusRect.Height() ;

		statusSeparator = GetDlgItem(IDC_STATUSSEPARATOR);
		statusSeparator->GetWindowRect(statusSeparatorRect);
		statusSeparator->MoveWindow(0, y - statusSeparatorRect.Height(), cx, 
									statusSeparatorRect.Height());

		y -= BUTTON_STATUS_MARGIN + statusSeparatorRect.Height();

		cancel = GetDlgItem(IDCANCEL);
		cancel->GetWindowRect(cancelRect);
		ok = GetDlgItem(IDOK);
		ok->GetWindowRect(okRect);

		x = (cx - okRect.Width() - cancelRect.Width() - BUTTON_SPACING) /2;

		ok->MoveWindow(x, y - okRect.Height(),
					   okRect.Width(), okRect.Height());

		x+= BUTTON_SPACING + okRect.Width();
		cancel->MoveWindow(x, y - cancelRect.Height(),
						   cancelRect.Width(), cancelRect.Height());


		y-= okRect.Height() + LIST_BUTTON_MARGIN;

		m_pOutlinerParent->MoveWindow(PICKER_LEFT_MARGIN, PICKER_TOP_MARGIN,
									cx - PICKER_LEFT_MARGIN - PICKER_RIGHT_MARGIN,
									y - PICKER_TOP_MARGIN);
	}

}


void CNameCompletion::OnOK() 
{
	CDialog::OnOK();
	MSG_ViewIndex *pIndex;
	int nCount;

	m_pOutliner->GetSelection( pIndex, nCount );

	if(m_pCookie && m_bFreeCookie)
	{
		AB_FreeNameCompletionCookie(m_pCookie);
	}

	m_pCookie = AB_GetNameCompletionCookieForIndex(m_pPickerPane, pIndex[0]);
	m_bFreeCookie = FALSE;

	CleanupOnClose();
}


void CNameCompletion::OnCancel() 
{
	CDialog::OnCancel();

	CleanupOnClose();
}

void CNameCompletion::OnUpdateProperties(CCmdUI *pCmdUI)
{
	DoUpdateCommand(pCmdUI, AB_PropertiesCmd);

}

void CNameCompletion::OnProperties()
{
	DoCommand(AB_PropertiesCmd);
}

void CNameCompletion::OnUpdateOnlineStatus(CCmdUI *pCmdUI)
{
 	pCmdUI->Enable(!NET_IsOffline());
}

void CNameCompletion::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_pPickerPane ) {
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
	if ( pane == (MSG_Pane*) m_pPickerPane ) {
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

	AB_LDAPSearchResultsAB2(m_pPickerPane, index, num);

/*
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
*/
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
	return (MSG_Pane*) m_pPickerPane;
}

void CNameCompletion::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if (notify == MSG_PaneNotifyStartSearching)
	{
		m_bSearching = TRUE;
		m_barStatus.StartAnimation();
	}
	else if(notify == MSG_PaneNotifyStopSearching )
	{
		m_bSearching = FALSE;
		m_barStatus.StopAnimation();
		if(::IsWindow(m_pOutlinerParent->m_hWnd))
		{
			//make sure outliner has the focus.
			m_pOutlinerParent->SetFocus();
		}


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

	HandleErrorReturn(AB_FinishSearchAB2(m_pPickerPane));

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
	//	HandleErrorReturn(AB_FinishSearch(m_addrBookPane, m_pCX->GetContext()));
		m_bSearching = FALSE;
		return;
	}

	// Begin Search
	m_barStatus.SetWindowText( szLoadString( IDS_SEARCHING ) );
	m_bSearching = TRUE;
	m_pOutliner->UpdateCount();
	m_pOutliner->SetFocus();

//	HandleErrorReturn(AB_SearchDirectory(m_addrBookPane, NULL));

}

void CNameCompletion::OnAddToAddressBook(UINT nID)
{
	int nPos = nID - FIRST_ADDSENDER_MENU_ID;

	XP_List *addressBooks = AB_AcquireAddressBookContainers(m_pCX->GetContext()); 

	if(addressBooks)
	{
		AB_ContainerInfo *pInfo = (AB_ContainerInfo*)XP_ListGetObjectNum(addressBooks, nPos);

		MSG_ViewIndex *indices = NULL;
		int count = 0;
		if (m_pOutliner)
			m_pOutliner->GetSelection(indices, count);


		HandleErrorReturn(AB_DragEntriesIntoContainer(m_pPickerPane, 
						  indices, count, pInfo, AB_Require_Copy));

		AB_ReleaseContainersList(addressBooks);

	}
}


void CNameCompletion::DoUpdateCommand(CCmdUI *pCmdUI, AB_CommandType cmd,BOOL bUseCheck)
{

	XP_Bool bSelectable;
	const char *displayString;
	XP_Bool bPlural;
	MSG_COMMAND_CHECK_STATE selected;

	MSG_ViewIndex *indices = NULL;
	int count = 0;
	if (m_pOutliner)
			m_pOutliner->GetSelection(indices, count);

	HandleErrorReturn(AB_CommandStatusAB2(m_pPickerPane,
		cmd, indices, count, &bSelectable, &selected, &displayString, &bPlural));

	pCmdUI->Enable(bSelectable);
	if(bUseCheck)
        pCmdUI->SetCheck(selected == MSG_Checked);
    else
        pCmdUI->SetRadio(selected == MSG_Checked);



}

void CNameCompletion::DoCommand(AB_CommandType cmd)
{
	MSG_ViewIndex *indices = NULL;
	int count = 0;
	if (m_pOutliner)
			m_pOutliner->GetSelection(indices, count);

	HandleErrorReturn(AB_CommandAB2(m_pPickerPane, cmd, indices, count));

}



CNameCompletionLineData::CNameCompletionLineData(AB_AttributeValue *pValues, 
												 int numColumns, int line)
{
	m_pValues = pValues;
	m_nNumColumns = numColumns;
	m_nLine = line;
}

CNameCompletionLineData::~CNameCompletionLineData()
{
	if(m_pValues)
	{
		AB_FreeEntryAttributeValues (m_pValues, m_nNumColumns);

	}
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
        m_pIUserImage->Initialize(IDB_DIRLIST,16,16);
    }
	m_iMysticPlane = 0;
	m_hFont = NULL;
	m_pLineData = NULL;
	m_pEntryAttrib = NULL;
	m_nNumColumns = 0;
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

	if(m_pEntryAttrib)
	{
		delete m_pEntryAttrib;
	}

	if (m_pLineData)
	{
		delete m_pLineData;
		m_pLineData = NULL;
	}

}



void CNameCompletionOutliner::UpdateCount( )
{
	uint32 count = 0;

	SetTotalLines(CASTINT(count));
}


void CNameCompletionOutliner::SetPane(MSG_Pane *pane)
{
	m_pane = pane;
	uint32 count = 0;

	if (m_pane) {

		//Get attributes for this pane
		m_nNumColumns = AB_GetNumColumnsForPane(pane); 

		if(m_pEntryAttrib)
		{
			delete m_pEntryAttrib;
		}

		m_pEntryAttrib = new AB_AttribID[m_nNumColumns];

		int numAttribs = (int) m_nNumColumns;

		AB_GetColumnAttribIDsForPane(pane,m_pEntryAttrib, &numAttribs);

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
			&&  num > 0) 
		{
			if(((CNameCompletion*)pParent)->IsSearching())
			{
				((CNameCompletion*)pParent)->SetSearchResults(where, num);
			}

			if(num > 0)
			{
				HandleInsert(where, num);
				//this is the first place I can figure out where to set the focus and have it
				//stick.  So, if it's item 0 then we know to select the first item and then 
				//set focus to us.
				if(where == 0 || where == 1)
				{
					SetFocus();
					if(GetTotalLines() == 1)
						SelectItem(0);
					else
						SelectItem(1);
				}
			}
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
	if ( (iColumn) != AB_ColumnID0 )
        return CMSelectOutliner::RenderData ( iColumn, rect, dc, text );
	int idxImage;

	AB_ContainerType type = AB_GetEntryContainerType(m_pane, m_pLineData->m_nLine);

    if (type == AB_MListContainer)
		idxImage = IDX_NAME_DIRMAILINGLIST;
	else if(type == AB_LDAPContainer)
		idxImage = IDX_NAME_DIRLDAPAB;
	else if(type == AB_PABContainer)
		idxImage = IDX_NAME_DIRPERSONALAB;

	m_pIUserImage->DrawImage ( idxImage,
		rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
	return TRUE;
}


int CNameCompletionOutliner::TranslateIcon ( void * pLineData )
{
	CNameCompletionLineData * pNameLineData = (CNameCompletionLineData*) pLineData;
	int idxImage = 0;

	AB_ContainerType type = AB_GetEntryContainerType(m_pane, pNameLineData->m_nLine);

    if (type == AB_MListContainer)
		idxImage = IDX_NAME_DIRMAILINGLIST;
	else if(type == AB_LDAPContainer)
		idxImage = IDX_NAME_DIRLDAPAB;
	else if(type == AB_PABContainer)
		idxImage = IDX_NAME_DIRPERSONALAB;
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

void CNameCompletionOutliner::AppendAddressBookMenuItem(CMenu *pMenu)
{
	MSG_ViewIndex *indices = NULL;
	int count = 0;
	GetSelection(indices, count);

	//if selection is not an LDAP item then don't add it to menu
	if(count > 0)
	{
		
		AB_ContainerType type = AB_GetEntryContainerType(m_pane, indices[0]);

		if(type != AB_LDAPContainer)
			return;

	}

	XP_List *addressBooks = AB_AcquireAddressBookContainers(m_pContext); 


	if(addressBooks)
	{
 
		int nCount = XP_ListCount(addressBooks);
		if(nCount == 1)
		{
			pMenu->AppendMenu( MF_BYPOSITION| MF_STRING, FIRST_ADDSENDER_MENU_ID, 
						"&Add to Address Book");

		}
		else
		{
			CMenu *pAddMenu = new CMenu;
			
			pAddMenu->CreatePopupMenu();

			pMenu->AppendMenu( MF_BYPOSITION| MF_STRING | MF_POPUP, (UINT)pAddMenu->m_hMenu,
						"&Add to Address Book");

			for(int i = 1; i <= nCount; i++)
			{
				AB_ContainerInfo *info = 
						(AB_ContainerInfo*)XP_ListGetObjectNum (addressBooks, i);

				AB_ContainerAttribValue *value;
				AB_GetContainerAttribute(info, attribName, &value);
						
				if(value != NULL)
				{
					pAddMenu->AppendMenu(MF_BYPOSITION | MF_STRING,  
										 FIRST_ADDSENDER_MENU_ID + i, value->u.string);

					AB_FreeContainerAttribValue(value);

				}


			}
		}

	
		AB_ReleaseContainersList(addressBooks);
	}


}

void CNameCompletionOutliner::PropertyMenu(int iSel, UINT flags)
{
	CMenu cmPopup;
	CString cs;
	
	if(cmPopup.CreatePopupMenu() == 0) 
		return;



    if (iSel < m_iTotalLines) {

		AppendAddressBookMenuItem(&cmPopup);

		cs.LoadString(IDS_GETINFO);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_PROPERTIES, cs);
	}

	//	Track the popup now.
	POINT pt = m_ptHit;
	ClientToScreen(&pt);

	cmPopup.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 
						   GetParent()->GetParent(), NULL);

    //  Cleanup
    cmPopup.DestroyMenu();



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
	if ( line >= m_iTotalLines) {
		if (m_pLineData) {
			delete m_pLineData;
		}

		m_pLineData = NULL;
		return NULL;
	}


	if (m_pLineData) {
		m_pLineData = NULL;
	}

	m_nNumColumns = AB_GetNumColumnsForPane(m_pane); 


	AB_AttributeValue *pValues;

	AB_GetEntryAttributesForPane(
		m_pane,
		line,
		m_pEntryAttrib, /* FE allocated array of attribs that you want */
		&pValues,
		&m_nNumColumns);

	m_pLineData = new CNameCompletionLineData(pValues, m_nNumColumns, line);
	return m_pLineData;

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
	if ((iColumn)== AB_ColumnID0)
		return ("");

	CNameCompletionLineData* pNameLineData = (CNameCompletionLineData*) pLineData;

	if (pNameLineData->m_pValues [iColumn].u.string && 
		*(pNameLineData->m_pValues [iColumn].u.string))
		return pNameLineData->m_pValues [iColumn].u.string;

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

	MSG_Pane *pPane = ((CNameCompletionOutliner*)m_pOutliner)->GetPane();

	int nNumColumns = AB_GetNumColumnsForPane(pPane); 

	char *displayString; 

	for(int i = 0; i < nNumColumns; i++)
	{
		AB_ColumnInfo * info = AB_GetColumnInfoForPane(pPane, (AB_ColumnID)i);


		if(i == AB_ColumnID0)
		{
			m_pOutliner->AddColumn ("",		0,			24, 0, ColumnFixed, 0, TRUE );
		}
		else
		{
			displayString = "";

			if(info != NULL)
			{
				displayString = info->displayString;
			}

			m_pOutliner->AddColumn (displayString,	i,		175, 0, ColumnVariable, 1500);
			
		}

		if(info)
		{
			AB_FreeColumnInfo(info);	
		}
	}

	m_pOutliner->SetHasPipes( FALSE );
	m_pOutliner->SetHasImageOnlyColumn(TRUE);

	m_pOutliner->SetVisibleColumns(DEF_VISIBLE_COLUMNS);
	m_pOutliner->LoadXPPrefs("name_completion.picker_columns_win");

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
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByTypeCmd, 0, 0);
			break;
		case ID_COLADDR_NAME:
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByFullNameCmd, 0, 0);
			break;
		case ID_COLADDR_NICKNAME:
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByNickname, 0, 0);
			break;
		case ID_COLADDR_LOCALITY:
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByLocality, 0, 0);
			break;
		case ID_COLADDR_COMPANY:
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByCompanyName, 0, 0);
			break;
		case ID_COLADDR_EMAIL:
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByEmailAddress, 0, 0);
			break;
		default:
			AB_CommandAB2(pOutliner->GetPane(), AB_SortByFullNameCmd, 0, 0);
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
		m_pOutliner->SaveXPPrefs("name_completion.picker_columns_win");
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

