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
#include "msgcom.h"
#include "wfemsg.h"
#include "dspppage.h"
#include "mailpriv.h" //uses forward reference to CDiskSpacePropertyPage
#include "nethelp.h"
#include "xp_help.h"
#include "prefapi.h"

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CMailNewsSplitter, CView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#define IDC_DOWNLOADNOW  20100
#define IDC_SYNCHRONIZE 20200
//Mail folder property page
CFolderPropertyPage::CFolderPropertyPage(CWnd *pWnd): 
	CNetscapePropertyPage( CFolderPropertyPage::IDD, 0 )
{
	m_folderInfo = NULL;
	m_pPane = NULL;
	m_pParent = (CNewsFolderPropertySheet*)pWnd; 
}

BEGIN_MESSAGE_MAP(CFolderPropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CFolderPropertyPage)
	ON_EN_CHANGE(IDC_EDIT1, OnChangeFolderName)
	ON_BN_CLICKED(IDC_BUTTON1, OnCleanUpWastedSpace)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CFolderPropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDiskSpacePropertyPage)
	DDX_Text(pDX, IDC_EDIT1, m_strFolderName);
	DDV_MaxChars(pDX, m_strFolderName, 50);
	//}}AFX_DATA_MAP
}


void CFolderPropertyPage::OnCleanUpWastedSpace()
{
	if (m_pParent)
		m_pParent->CleanUpWastedSpace();
}

void CFolderPropertyPage::SetFolderInfo( MSG_FolderInfo *folderInfo , MSG_Pane *pPane)
{
	m_folderInfo = folderInfo;
	m_pPane = pPane;
}

BOOL CFolderPropertyPage::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();
	MSG_FolderLine folderLine;

	char buff[50];
	CWnd *widget = NULL;

	if ( MSG_GetFolderLineById( WFE_MSGGetMaster(), m_folderInfo, &folderLine ) ) {
		widget = GetDlgItem( IDC_EDIT1 ); 
		if(widget)
			widget->SetWindowText( folderLine.prettyName != NULL ? folderLine.prettyName : folderLine.name );
	}

	
	int nLen = 49;
	if ( PREF_NOERROR == PREF_GetCharPref("network.hosts.pop_server",buff,&nLen) )
		SetDlgItemText(IDC_MAIL_SERVER_NAME,buff);

	
	widget = GetDlgItem(IDC_STATIC_UNREAD);
	if (widget)
	{
		if (folderLine.unseen > 0)
		widget->SetWindowText(itoa(folderLine.unseen, buff, 10));
	}

	widget = GetDlgItem(IDC_STATIC_TOTAL);
	if (widget)
		widget->SetWindowText(itoa(folderLine.total, buff, 10));
	
	int32 nSpaceUsed = MSG_GetFolderSizeOnDisk (m_folderInfo);
	int nSpaceWasted = 0;

	if (folderLine.deletedBytes != 0 && (nSpaceUsed != 0))//never divide by zero
		nSpaceWasted = (int)(100 *((double)(folderLine.deletedBytes)/(double)nSpaceUsed) );
	else
		nSpaceWasted = 0;

	//never calculate more than a 100% 
	nSpaceWasted = nSpaceWasted > 100 ? 100 : nSpaceWasted;  

	widget = GetDlgItem(IDC_PERCENT_WASTED);
	if (widget)
	{
		CString strSpaceWasted = itoa(nSpaceWasted, buff, 10) + (CString)"%";
		widget->SetWindowText(strSpaceWasted);
	}

	widget = GetDlgItem(IDC_USED_SPACE);
	if (widget)
	{
		char buff[30];
		//convert bytes to Kilobytes and display in floating point format with 
		//up to 2 pricision points
		sprintf(buff,"%.2f kbytes",(float)nSpaceUsed/1000);	
		widget->SetWindowText(buff);
	}

	if (folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX)
	{
		if (widget = GetDlgItem(IDC_BUTTON1)) 
			widget->ShowWindow(SW_HIDE);	
	}
	return ret;
}

void CFolderPropertyPage::OnChangeFolderName()
{
	UpdateData();
}

void CFolderPropertyPage::OnOK()
{
	CNetscapePropertyPage::OnOK();
	MSG_RenameMailFolder (m_pPane,m_folderInfo,m_strFolderName);
}

///////////////////////////////////////////////////////////////////////////////////////////
//CNewsGeneralPropertyPage
//The general news property page

CNewsGeneralPropertyPage::CNewsGeneralPropertyPage(CNewsFolderPropertySheet *pParent ): 
	CNetscapePropertyPage( CNewsGeneralPropertyPage::IDD )
{
	m_folderInfo = NULL;
	m_pContext = NULL;
	m_pParent = pParent;

}

BEGIN_MESSAGE_MAP(CNewsGeneralPropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CNewsGeneralPropertyPage)
	ON_BN_CLICKED(IDC_DOWNLOAD_NOW, OnDownLoadButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CNewsGeneralPropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewsGeneralPropertyPage)
	DDX_Check(pDX, IDC_CHECK_RECEIVE_HTML, m_bCanReceiveHTML);
	//}}AFX_DATA_MAP
}

//called just after construction
void CNewsGeneralPropertyPage::SetFolderInfo( MSG_FolderInfo *folderInfo, MWContext *pContext )
{
	m_folderInfo = folderInfo;
	m_pContext = pContext;
}


BOOL CNewsGeneralPropertyPage::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();
	MSG_FolderLine folderLine;
	char buff[20];
	CWnd *widget = NULL;

	if ( MSG_GetFolderLineById( WFE_MSGGetMaster(), m_folderInfo, &folderLine ) ) {
		widget = GetDlgItem( IDC_PRETTY_NAME ); 
		if(widget)
			widget->SetWindowText( folderLine.prettyName != NULL ? folderLine.prettyName : folderLine.name );
		widget = GetDlgItem(IDC_UGLY_NAME);

		if (folderLine.prettyName != NULL)
		{
			CString strName = "(";
			strName += folderLine.name + (CString)")";
			if (widget) widget->SetWindowText(strName);
		}
		else
			if (widget) widget->ShowWindow(SW_HIDE);

	}

	widget = GetDlgItem(IDC_NEWS_SERVER);
	if (widget) 
	{
		MSG_NewsHost* pNewsHost =  MSG_GetNewsHostForFolder(m_folderInfo);
		if (pNewsHost)
		{
			widget->SetWindowText(MSG_GetNewsHostName(pNewsHost));
		}
	}

	widget = GetDlgItem(IDC_STATIC_UNREAD);
	if (widget)
		widget->SetWindowText(itoa(folderLine.unseen, buff, 10));

	widget = GetDlgItem(IDC_STATIC_TOTAL);
	if (widget)
		widget->SetWindowText(itoa(folderLine.total, buff, 10));

	CheckDlgButton(IDC_CHECK_RECEIVE_HTML, m_bCanReceiveHTML = MSG_IsHTMLOK(WFE_MSGGetMaster(), m_folderInfo) );

	return ret;
}

void CNewsGeneralPropertyPage::OnDownLoadButton()
{
	m_pParent->OnDownLoadButton();
}

void CNewsGeneralPropertyPage::OnOK()
{
	CNetscapePropertyPage::OnOK();
	MSG_SetIsHTMLOK(WFE_MSGGetMaster(), m_folderInfo,
						   m_pContext, (XP_Bool)m_bCanReceiveHTML);
}


////////////////////////////////////////////////////////////////////////////////////
//CNewsHostGeneralPropertyPage
//News Host general property page
////////////////////////////////////
CNewsHostGeneralPropertyPage::CNewsHostGeneralPropertyPage( ): 
	CNetscapePropertyPage( CNewsHostGeneralPropertyPage::IDD, 0 )
{
	m_nRadioValue = 1;
	m_bCanReceiveHTML = TRUE;
	m_pNewsHost=NULL;
	m_folderInfo=NULL;
}

BEGIN_MESSAGE_MAP(CNewsHostGeneralPropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CNewsHostGeneralPropertyPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CNewsHostGeneralPropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewsHostGeneralPropertyPage)
	DDX_Radio(pDX, IDC_RADIO1, m_nRadioValue);
	//}}AFX_DATA_MAP
}
	
void CNewsHostGeneralPropertyPage::SetFolderInfo( MSG_FolderInfo *folderInfo , MSG_NewsHost *pNewsHost)
{
	m_folderInfo = folderInfo;
	m_pNewsHost = pNewsHost;
}

BOOL CNewsHostGeneralPropertyPage::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();
	CString strItemText;
	char buffer[100];

	//Set the server name
	CWnd *widget = GetDlgItem( IDC_SERVER_NAME ); 
	if(widget)
		widget->SetWindowText(MSG_GetNewsHostName(m_pNewsHost));
    //set the port name
	widget = GetDlgItem( IDC_PORT_NUMBER );
	int32 nPort = MSG_GetNewsHostPort(m_pNewsHost);
	if (widget)
		widget->SetWindowText(itoa(nPort,buffer,10));
	//set the security type
	widget = GetDlgItem( IDC_SECURITY_TYPE );	
	if ( MSG_IsNewsHostSecure(m_pNewsHost))
	{
		strItemText.LoadString(IDS_IS_ENCRYPTED);
	}
	else
	{
		strItemText.LoadString(IDS_NOT_ENCRYPTED);
	}
	if (widget)
		widget->SetWindowText(strItemText);

	//set the authentication type.
	//We are transposing values here because the radio buttons are in reverse  
	//order compared to the logic return value.
	m_nRadioValue     = (MSG_GetNewsHostPushAuth (m_pNewsHost) == 0) ? 1: 0 ;
	UpdateData(FALSE);

	return ret;
}

void CNewsHostGeneralPropertyPage::OnOK()
{
	CNetscapePropertyPage::OnOK();
	//We are transposing values here because the radio buttons are in reverse  
	//ordecompared to the logic return value.
	XP_Bool bSetValue = (m_nRadioValue == 0) ? 1: 0;
	MSG_SetNewsHostPushAuth (m_pNewsHost, bSetValue);
}
//////////

/////////////////////////////////////////////////////////////////////////////////////////
//CNewsFolderPropertySheet

CNewsFolderPropertySheet::CNewsFolderPropertySheet(LPCTSTR pszCaption, CWnd *pParent)
						 : CNetscapePropertySheet(pszCaption, pParent)
{
	m_pFolderPage=NULL;
	m_pNewsFolderPage=NULL;
	m_pDiskSpacePage=NULL;
	m_pDownLoadPageMail=NULL;
	m_pDownLoadPageNews=NULL;
	m_pNewsHostPage = NULL;

	m_bDownLoadNow = FALSE;
	m_bSynchronizeNow = FALSE;
	m_bCleanUpNow = FALSE;
	m_pParent = pParent;
}

CNewsFolderPropertySheet::~CNewsFolderPropertySheet()
{
	if (m_pFolderPage)
		delete m_pFolderPage;
	if (m_pNewsFolderPage)
		delete m_pNewsFolderPage;
	if (m_pDiskSpacePage)
		delete m_pDiskSpacePage;
	if (m_pDownLoadPageMail)
		delete m_pDownLoadPageMail;
	if (m_pDownLoadPageNews)
		delete m_pDownLoadPageNews;
	if (m_pNewsHostPage)
		delete m_pNewsHostPage;
}

void CNewsFolderPropertySheet::OnHelp()
{
	if ((GetActivePage() == m_pFolderPage))
		NetHelp(HELP_MAIL_FOLDER_PROPS_GENERAL);
	
	else if (GetActivePage() == m_pDownLoadPageMail)
		NetHelp(HELP_MAIL_FOLDER_PROPS_GENERAL);

	else if (GetActivePage() == m_pNewsFolderPage )
		NetHelp(HELP_NEWS_DISCUSION_GENERAL);

	else if (GetActivePage() == m_pNewsHostPage)
		NetHelp(HELP_DISCUSSION_HOST_PROPERTIES);

	else if (GetActivePage() == m_pDiskSpacePage)
		NetHelp(HELP_NEWS_DISCUSION_DISKSPACE);

	else if (GetActivePage() == m_pDownLoadPageNews)
		NetHelp(HELP_NEWS_DISCUSION_DOWNLOAD);
}

void CNewsFolderPropertySheet::OnDownLoadButton()
{
	m_bDownLoadNow = TRUE;
	CNetscapePropertySheet::OnOK();
}

void CNewsFolderPropertySheet::OnSynchronizeButton()
{
	m_bSynchronizeNow = TRUE;
	CNetscapePropertySheet::OnOK();
}

void CNewsFolderPropertySheet::CleanUpWastedSpace()
{
	m_bCleanUpNow = TRUE;
	CNetscapePropertySheet::OnOK();
}

BEGIN_MESSAGE_MAP(CNewsFolderPropertySheet, CNetscapePropertySheet)
    ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(IDC_DOWNLOADNOW,OnDownLoadButton)
	ON_COMMAND(IDC_SYNCHRONIZE,OnSynchronizeButton)
END_MESSAGE_MAP()

//End CNewsFolderPropertySheet
////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
//General page for attachments

CAttachmentGeneralPage::CAttachmentGeneralPage(LPCTSTR lpszName, LPCTSTR lpszType, LPCTSTR lpszDescription):
	CNetscapePropertyPage(CAttachmentGeneralPage::IDD)
{
	m_csName = lpszName ? lpszName : "";
	m_csType = lpszType ? lpszType : "";
	m_csDescription = lpszDescription ? lpszDescription : "";
}

void CAttachmentGeneralPage::DoDataExchange(CDataExchange* pDX)
{
	DDX_Text(pDX, IDC_STATIC1, m_csName);
	DDX_Text(pDX, IDC_STATIC2, m_csType);
	DDX_Text(pDX, IDC_STATIC3, m_csDescription);
}

/////////////////////////////////////////////////////////////////////////
// Property sheet for attachments

CAttachmentSheet::CAttachmentSheet(CWnd *pParentWnd,
								   LPCTSTR lpszName, LPCTSTR lpszType, LPCTSTR lpszDescription):
CNetscapePropertySheet(szLoadString(IDS_ATTACHMENTPROP), pParentWnd, 0)
{
	m_pGeneral = new CAttachmentGeneralPage(lpszName, lpszType, lpszDescription);
	AddPage(m_pGeneral);
}

CAttachmentSheet::~CAttachmentSheet()
{
	delete m_pGeneral;
}

/////////////////////////////////////////////////////////////////////
//
// CThreadStatusBar
//
// Status bar with little "expando" widget on the left
//

CThreadStatusBar::CThreadStatusBar()
{
	VERIFY( m_hbmExpando = ::LoadBitmap( AfxGetResourceHandle(), 
										 MAKEINTRESOURCE( IDB_VFLIPPY ) ));
	BITMAP bm;
	::GetObject( m_hbmExpando, sizeof( bm ), &bm );
	m_sizeExpando.cx = bm.bmWidth / 4;
	m_sizeExpando.cy = bm.bmHeight;

	m_bDepressed = FALSE;
	m_bExpandoed = FALSE;
    
    m_iStatBarPaneWidth = 0;
}

CThreadStatusBar::~CThreadStatusBar()
{
	if (m_hbmExpando) {
		VERIFY( ::DeleteObject( m_hbmExpando ));
	}
}

BOOL CThreadStatusBar::Create( CWnd *pParent)
{
	BOOL bRtn = CNetscapeStatusBar::Create( pParent, TRUE, TRUE );
	
	return(bRtn);
}

void CThreadStatusBar::Expando( BOOL bExpando )
{
	if ( bExpando != m_bExpandoed ) {
		m_bExpandoed = bExpando;
		Invalidate();
	}
}

void CThreadStatusBar::SetupMode()
{
	CNetscapeStatusBar::SetupMode();

	int idx = CommandToIndex(IDS_EXPANDO);
	if (idx > -1)
		SetPaneInfo( idx, IDS_EXPANDO, SBPS_DISABLED|SBPS_NOBORDERS, m_sizeExpando.cx);
}

BEGIN_MESSAGE_MAP( CThreadStatusBar, CNetscapeStatusBar )
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CThreadStatusBar::OnLButtonDown( UINT nFlags, CPoint point )
{
	RECT rect;
	GetItemRect(CommandToIndex(IDS_EXPANDO), &rect);

	if ( PtInRect( &rect, point ) ) {
		SetCapture();
		m_bDepressed = TRUE;
		InvalidateRect( &rect );
	}
	CNetscapeStatusBar::OnLButtonDown( nFlags, point );
}

void CThreadStatusBar::OnMouseMove( UINT nFlags, CPoint point )
{
	if ( GetCapture() == this ) {
		RECT rect;
		GetItemRect(CommandToIndex(IDS_EXPANDO), &rect);

		BOOL bDepressed = PtInRect( &rect, point );

		if ( bDepressed != m_bDepressed ) {
			m_bDepressed = bDepressed;
			InvalidateRect( &rect );
		}
	}
	CNetscapeStatusBar::OnMouseMove( nFlags, point );
}

void CThreadStatusBar::OnLButtonUp( UINT nFlags, CPoint point )
{
	if ( GetCapture() == this ) {
		ReleaseCapture();

		RECT rect;
		GetItemRect(CommandToIndex(IDS_EXPANDO), &rect);

		BOOL bDepressed = PtInRect( &rect, point );

		if ( bDepressed && m_bExpandoed ) {
			GetParentFrame()->PostMessage( WM_COMMAND, (WPARAM) ID_VIEW_MESSAGE, 
										   (LPARAM) 0);
		}
		m_bDepressed = FALSE;
	}

	CNetscapeStatusBar::OnLButtonUp( nFlags, point );
}

void CThreadStatusBar::OnPaint()
{
	CNetscapeStatusBar::OnPaint();

	if ( m_bExpandoed ) {
		int idx = CommandToIndex(IDS_EXPANDO);
		if ( idx > -1 ) {
			RECT rect; 
			GetItemRect(idx, &rect);
	
			HDC hdcClient = ::GetDC( m_hWnd );
			HDC hdcBitmap = ::CreateCompatibleDC( hdcClient );
	
			HBITMAP hbmOld = (HBITMAP) ::SelectObject( hdcBitmap, m_hbmExpando );
	
			FEU_TransBlt( hdcClient, 
						  rect.left, rect.top, 
						  m_sizeExpando.cx, m_sizeExpando.cy,
						  hdcBitmap,
						  m_bDepressed ? m_sizeExpando.cx : 0, 0 ,WFE_GetUIPalette(GetParentFrame())
						 );
	
			::SelectObject( hdcBitmap, hbmOld );
			VERIFY( ::DeleteDC( hdcBitmap ));
			::ReleaseDC( m_hWnd, hdcClient );  
		}
	}
}

/////////////////////////////////////////////////////////////////////
//
// CProgressDialog
//
// Dialog for stand-along mail downloading
//

CProgressDialog::CProgressDialog( CWnd *pParent, MSG_Pane *parentPane, 
	PROGRESSCALLBACK callback, void * closure, char * pszTitle,
	PROGRESSCALLBACK cbDone):
	CStubsCX( MailCX, MWContextMailNewsProgress )
{
	m_pszTitle = pszTitle ? XP_STRDUP(pszTitle) : NULL;
	m_lPercent = 0;
	m_pPane= MSG_CreateProgressPane( GetContext(), WFE_MSGGetMaster(), parentPane );

	m_pParent = pParent;
	m_cbDone = cbDone;
	m_closure = closure;

	MSG_SetFEData( m_pPane, (LPVOID) (LPUNKNOWN) (LPMAILFRAME) this );

	if (Create( CProgressDialog::IDD, pParent )) {
		if (callback)
			(*callback)(m_hWnd, m_pPane, closure);
	} else {
		if (m_cbDone)
			(*m_cbDone)(m_hWnd, m_pPane, closure);
	}
}

STDMETHODIMP CProgressDialog::QueryInterface(REFIID refiid, LPVOID * ppv)
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

STDMETHODIMP_(ULONG) CProgressDialog::AddRef(void)
{
	return 0; // Not a real component
}

STDMETHODIMP_(ULONG) CProgressDialog::Release(void)
{
	return 0; // Not a real component
}

void CProgressDialog::PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							    MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if ( notify == MSG_PanePastPasswordCheck ) {
		ShowWindow( SW_SHOWNA );
		UpdateWindow();
	}
	else if (notify == MSG_PaneProgressDone) {
		DestroyWindow();
	}
}

void CProgressDialog::AttachmentCount(MSG_Pane *messagepane, void* closure,
									   int32 numattachments, XP_Bool finishedloading)
{
}

void CProgressDialog::UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure)
{
}

BEGIN_MESSAGE_MAP( CProgressDialog, CDialog )
	ON_WM_DESTROY()
	ON_MESSAGE(WM_REQUESTPARENT,OnRequestParent)
END_MESSAGE_MAP()

LONG CProgressDialog::OnRequestParent(WPARAM,LPARAM)
{
	return (LONG) m_pParent;
}

BOOL CProgressDialog::OnInitDialog( )
{
	CDialog::OnInitDialog();
	m_progressMeter.SubclassDlgItem( IDC_PROGRESS, this );
	if (m_pszTitle)
		SetWindowText(m_pszTitle);
	return FALSE;
}

void CProgressDialog::OnCancel()
{
	if (XP_IsContextStoppable(GetContext()))
		XP_InterruptContext(GetContext());
	else
		DestroyWindow();
}

void CProgressDialog::OnDestroy()
{
	if (m_cbDone)
		(*m_cbDone)(m_hWnd, m_pPane, m_closure);

	CDialog::OnDestroy();

	if ( m_pPane )
		MSG_DestroyPane( m_pPane );

	if(!IsDestroyed()) {
		DestroyContext();
	}
}

void CProgressDialog::SetProgressBarPercent(MWContext *pContext, int32 lPercent )
{
	//	Ensure the safety of the value.

	lPercent = lPercent < 0 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_lPercent == lPercent ) {
		return;
	}

	m_lPercent = lPercent;
	m_progressMeter.StepItTo( CASTINT(lPercent) );

	CWnd *widget = GetDlgItem( IDC_STATIC2 );
	CString cs;
	cs.Format("%d%%", CASTINT(lPercent));
	widget->SetWindowText(cs);
}

void CProgressDialog::Progress(MWContext *pContext, const char *pMessage)
{
	CWnd *pWidget = GetDlgItem( IDC_STATIC1 );
	pWidget->SetWindowText( pMessage );
	pWidget->UpdateWindow();
}

int32 CProgressDialog::QueryProgressPercent()
{
	return m_lPercent;
}

void CProgressDialog::SetDocTitle( MWContext *pContext, char *pTitle )
{
}

void CProgressDialog::StartAnimation()
{
}

void CProgressDialog::StopAnimation()
{
}

void CProgressDialog::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CStubsCX::AllConnectionsComplete(pContext);

	DestroyWindow();
}

void CProgressDialog::UpdateStopState( MWContext *pContext )
{
}

CWnd *CProgressDialog::GetDialogOwner() const {
	return (CDialog *) this;
}

/////////////////////////////////////////////////////////////////////
//
// CNewFolderDialog
//
// Dialog for mail folder creation
//

CNewFolderDialog::CNewFolderDialog( CWnd *pParent, MSG_Pane *pPane, 
								    MSG_FolderInfo *folderInfo ):
	CDialog( IDD, pParent )
{
	MWContext *pXPCX;
	MWContextType saveType;

	m_bEnabled = TRUE;
	m_pPane = NULL;

	if (pPane)
	{
		pXPCX = MSG_GetContext( pPane );
		// Since the progress pane changes it's context's type,
		// Save it
		saveType = pXPCX->type;

		m_pPane= MSG_CreateProgressPane( pXPCX, WFE_MSGGetMaster(), pPane );
		MSG_SetFEData( m_pPane, (LPVOID) (LPUNKNOWN) this );
	}
	
	m_pParentFolder = folderInfo;

	DoModal();

	// Restore true context type
	if (pPane)
		pXPCX->type = saveType;	
}

BOOL CNewFolderDialog::OnInitDialog( )
{
	BOOL res = CDialog::OnInitDialog();

	if ( res ) {
		// Subclass folder combo
		m_wndCombo.SubclassDlgItem( IDC_COMBO1, this );
		m_wndCombo.PopulateMail( WFE_MSGGetMaster() );
		m_wndCombo.SetCurSel(0);
		for ( int i = 0; i < m_wndCombo.GetCount(); i++ ) {
			if ( (MSG_FolderInfo *) m_wndCombo.GetItemData( i ) == m_pParentFolder ) {
				m_wndCombo.SetCurSel(i);
				break;
			}
		}
	}

	return res;
}

void CNewFolderDialog::OnCancel()
{
	CDialog::OnCancel();
}

void CNewFolderDialog::OnOK()
{
	CString csName;
	CWnd *widget;
	CComboBox *combo = (CComboBox *) GetDlgItem( IDC_COMBO1 );

	widget = GetDlgItem( IDC_EDIT1 );
	widget->GetWindowText( csName );

	m_pParentFolder = (MSG_FolderInfo *) combo->GetItemData( combo->GetCurSel() );

	if ( m_pParentFolder && !csName.IsEmpty() ) {
		int err;
		if (m_pPane)
		{
			err = MSG_CreateMailFolderWithPane( m_pPane, WFE_MSGGetMaster(), 
				m_pParentFolder, csName );
		}
		else
		{
			err = MSG_CreateMailFolder (WFE_MSGGetMaster(), m_pParentFolder, csName);
		}
		if ( ! err ) {
			m_bEnabled = FALSE;
		}
	} else {
		MessageBox( szLoadString(IDS_WHYCREATIONFAILED), 
					szLoadString(IDS_CREATIONFAILED),
					MB_ICONEXCLAMATION|MB_OK );
	}
}

STDMETHODIMP CNewFolderDialog::QueryInterface(REFIID refiid, LPVOID * ppv)
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

STDMETHODIMP_(ULONG) CNewFolderDialog::AddRef(void)
{
	return 0; // Not a real component
}

STDMETHODIMP_(ULONG) CNewFolderDialog::Release(void)
{
	return 0; // Not a real component
}

void CNewFolderDialog::PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
								    MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if ( notify == MSG_PaneNotifySelectNewFolder ) {
		EndDialog( IDOK );
	}
}

void CNewFolderDialog::AttachmentCount(MSG_Pane *messagepane, void* closure,
									   int32 numattachments, XP_Bool finishedloading)
{
}

void CNewFolderDialog::UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure)
{
}

BEGIN_MESSAGE_MAP( CNewFolderDialog, CDialog )
	ON_WM_DESTROY()
	ON_UPDATE_COMMAND_UI( IDOK, OnEnable )
	ON_UPDATE_COMMAND_UI( IDCANCEL, OnEnable )
	ON_UPDATE_COMMAND_UI( IDC_EDIT1, OnEnable )
	ON_UPDATE_COMMAND_UI( IDC_COMBO1, OnEnable )
END_MESSAGE_MAP()

void CNewFolderDialog::OnDestroy()
{
	if (m_pPane)
	{
		MSG_SetFEData( m_pPane, NULL );
		MSG_DestroyPane( m_pPane );
	}
}

void CNewFolderDialog::OnEnable( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( m_bEnabled );
}


/////////////////////////////////////////////////////////////////////
//
// CPrefNewFolderDialog
//
// Dialog for mail folder creation for preference
//

CPrefNewFolderDialog::CPrefNewFolderDialog( CWnd *pParent, MSG_FolderInfo *pFolderInfo  ):
	CDialog( IDD, pParent )
{
	m_pFolder = pFolderInfo;
	m_bCreating = FALSE;
}

BOOL CPrefNewFolderDialog::OnInitDialog( )
{
	BOOL res = CDialog::OnInitDialog();

	if ( res ) {
		// Subclass folder combo
		m_wndCombo.SubclassDlgItem( IDC_COMBO1, this );
		m_wndCombo.PopulateMail( WFE_MSGGetMaster() );
		m_wndCombo.SetCurSel(0);
		for ( int i = 0; i < m_wndCombo.GetCount(); i++ ) {
			if ( (MSG_FolderInfo *) m_wndCombo.GetItemData( i ) == m_pFolder ) {
				m_wndCombo.SetCurSel(i);
				break;
			}
		}
	}

	return res;
}

void CPrefNewFolderDialog::OnCancel()
{
	CDialog::OnCancel();
}

static void _CreateFolderCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
	if (::IsWindow(hwnd)) {
		::ShowWindow(hwnd,SW_SHOW);
		::UpdateWindow(hwnd);
	}

	if (pane != NULL)
	{
		HWND dialog = (HWND)closure;
		char szName[256];
		HWND widget = GetDlgItem(dialog, IDC_EDIT1);
		::GetWindowText(widget, szName, 255);

		HWND combo = ::GetDlgItem(dialog, IDC_COMBO1 );
		int nIndex = SendMessage(combo, CB_GETCURSEL, 0, 0);
		MSG_FolderInfo *pFolder = (MSG_FolderInfo *)SendMessage(combo, 
											CB_GETITEMDATA, nIndex, 0);
		int err = MSG_CreateMailFolderWithPane(pane, WFE_MSGGetMaster(), 
												pFolder, szName);
	}
}

static void _CreateFolderDoneCallback(HWND hwnd, MSG_Pane *pane, void *closure)
{
	::PostMessage((HWND)closure, WM_COMMAND, (WPARAM)IDOK, (LPARAM) 0);
}

void CPrefNewFolderDialog::OnOK()
{
	CDialog::OnOK();

	MSG_Master* pMaster = WFE_MSGGetMaster();
	int32 iLines = MSG_GetFolderChildren (pMaster, m_pFolder, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];
	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		CString csName;
		GetDlgItem( IDC_EDIT1 )->GetWindowText( csName );

		MSG_GetFolderChildren(pMaster, m_pFolder, ppFolderInfo, iLines);
		m_pFolder = NULL;
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (pMaster, ppFolderInfo[i], &folderLine)) 
			{
				if (!XP_FILENAMECMP(LPCTSTR(csName), folderLine.name))
				{
					m_pFolder = ppFolderInfo[i];
					break;
				}
			}
		}
		delete [] ppFolderInfo;
	}
}

BOOL CPrefNewFolderDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
	if (!m_bCreating && (IDOK == LOWORD(wParam) && HIWORD(wParam) == BN_CLICKED))
#else
	if (!m_bCreating && (IDOK == wParam && HIWORD(lParam) == BN_CLICKED))
#endif
	{
		CString csName;
		GetDlgItem( IDC_EDIT1 )->GetWindowText( csName );

		CComboBox *combo = (CComboBox *) GetDlgItem( IDC_COMBO1 );
		m_pFolder = (MSG_FolderInfo *)combo->GetItemData( combo->GetCurSel() );

		if ( m_pFolder && !csName.IsEmpty() ) 
		{
			m_bCreating = TRUE;
			new CProgressDialog(this, NULL, _CreateFolderCallback,
						this->GetSafeHwnd(), "Createing Mail Folder", _CreateFolderDoneCallback);
			return TRUE;
		}
		else 
		{
			MessageBox( szLoadString(IDS_WHYCREATIONFAILED), 
						szLoadString(IDS_CREATIONFAILED),
						MB_ICONEXCLAMATION|MB_OK );
			return TRUE;
		}
	}
	else
		return CDialog::OnCommand(wParam, lParam);
}

BEGIN_MESSAGE_MAP( CPrefNewFolderDialog, CDialog )
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////
// CMailNewsSplitter
//
// for close pane widge, so we can use one bitmap for all 
HBITMAP		m_hHCloseNBmp = NULL;
HBITMAP		m_hHCloseHBmp = NULL;
HBITMAP		m_hVCloseNBmp = NULL;
HBITMAP		m_hVCloseHBmp = NULL;
HBITMAP		m_hHShowNBmp = NULL;
HBITMAP		m_hHShowHBmp = NULL;
HBITMAP		m_hVShowNBmp = NULL;
HBITMAP		m_hVShowHBmp = NULL;
int			nHCloseNRefCount = 0;
int			nHCloseHRefCount = 0;
int			nVCloseNRefCount = 0;
int			nVCloseHRefCount = 0;
int			nHShowNRefCount = 0;
int			nHShowHRefCount = 0;
int			nVShowNRefCount = 0;
int			nVShowHRefCount = 0;

#define SLIDER_PIXELS		7	   // slider width (both vertical and horizontal
#define SLIDER_MARGIN		4	   // slider margin when close pane
#define ZAP_HEIGHT			118	   // zap widge height
#define ZAP_MARGIN			32	   // space to close

CMailNewsSplitter::CMailNewsSplitter()
{
	m_bEraseBackground = TRUE;

	m_pWnd1 = NULL;
	m_pWnd2 = NULL;
	m_bVertical = TRUE;
	m_bTrackSlider = FALSE;
	m_nPaneSize = -1;
	m_nPrevSize = -1;

	::SetRectEmpty( &m_rcSlider );

	m_hSliderBrush = NULL;

	m_nSliderWidth = SLIDER_PIXELS;
	m_bZapped = FALSE;
	m_bZapperDown = FALSE;
	m_bDoubleClicked = FALSE;
	m_bMouseMove = FALSE;
}

CMailNewsSplitter::~CMailNewsSplitter()
{
	if (m_hSliderBrush)
		VERIFY(::DeleteObject( (HGDIOBJ) m_hSliderBrush ));

	DeleteBitmaps();

}

void CMailNewsSplitter::CreateBitmaps(HDC hDC)
{
	HINSTANCE hInst = AfxGetResourceHandle( );
	HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame( ));
	COLORREF rgbColor = RGB(255, 0, 255);

	if (nHCloseNRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hHCloseNBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILHCLOSEPANE_N);
	}
	nHCloseNRefCount++;

	if (nHCloseHRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hHCloseHBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILHCLOSEPANE_H);
	}
	nHCloseHRefCount++;

	if (nVCloseNRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hVCloseNBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILVCLOSEPANE_N);
	}
	nVCloseNRefCount++;

	if (nVCloseHRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hVCloseHBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILVCLOSEPANE_H);
	}
	nVCloseHRefCount++;

	if (nHShowNRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hHShowNBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILHSHOWPANE_N);
	}
	nHShowNRefCount++;

	if (nHShowHRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hHShowHBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILHSHOWPANE_H);
	}
	nHShowHRefCount++;

	if (nVShowNRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hVShowNBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILVSHOWPANE_N);
	}
	nVShowNRefCount++;

	if (nVShowHRefCount == 0)
	{
		WFE_InitializeUIPalette(hDC);
		m_hVShowHBmp = WFE_LoadTransparentBitmap(hInst, hDC, 
				sysInfo.m_clrBtnFace, rgbColor, hPalette, 
				IDB_MAILVSHOWPANE_H);
	}
	nVShowHRefCount++;

}

void CMailNewsSplitter::DeleteBitmaps()
{
	nHCloseNRefCount--;
	if (nHCloseNRefCount == 0)
	{
		if (m_hHCloseNBmp)
			DeleteObject(m_hHCloseNBmp);
	}
	nHCloseHRefCount--;
	if (nHCloseHRefCount == 0)
	{
		if (m_hHCloseHBmp)
			DeleteObject(m_hHCloseHBmp);
	}
	nVCloseNRefCount--;
	if (nVCloseNRefCount == 0)
	{
		if (m_hVCloseNBmp)
			DeleteObject(m_hVCloseNBmp);
	}
	nVCloseHRefCount--;
	if (nVCloseHRefCount == 0)
	{
		if (m_hVCloseHBmp)
			DeleteObject(m_hVCloseHBmp);
	}
	nHShowNRefCount--;
	if (nHShowNRefCount == 0)
	{
		if (m_hHShowNBmp)
			DeleteObject(m_hHShowNBmp);
	}
	nHShowHRefCount--;
	if (nHShowHRefCount == 0)
	{
		if (m_hHShowHBmp)
			DeleteObject(m_hHShowHBmp);
	}
	nVShowNRefCount--;
	if (nVShowNRefCount == 0)
	{
		if (m_hVShowNBmp)
			DeleteObject(m_hVShowNBmp);
	}
	nVShowHRefCount--;
	if (nVShowHRefCount == 0)
	{
		if (m_hVShowHBmp)
			DeleteObject(m_hVShowHBmp);
	}
}

void CMailNewsSplitter::AddPanes(CWnd *pWnd1, CWnd *pWnd2, int nSize, BOOL bVertical)
{
	m_pWnd1 = pWnd1;
	m_pWnd2 = pWnd2;
	m_nPaneSize  = nSize;
	m_bVertical = bVertical;
}

void CMailNewsSplitter::AddOnePane(CWnd *pWnd, BOOL bFirstPane, BOOL bVertical)
{
	if (!m_pWnd1 || m_pWnd2 == pWnd)
		return;

	m_bVertical = bVertical;

	if (bFirstPane)
	{
		m_pWnd2 = m_pWnd1;
		m_pWnd1 = pWnd;
	}
	else
		m_pWnd2 = pWnd;

	UpdateSplitter();
}

void CMailNewsSplitter::RemoveOnePane(CWnd *pWnd)
{
	if (m_pWnd1 == pWnd) //remove first pane
	{
		m_pWnd1 = m_pWnd2;
		m_pWnd2 = pWnd;
	}
	else
	{
		ASSERT(m_pWnd2 == pWnd);
	}
	m_pWnd2->MoveWindow(0, 0, 0, 0, TRUE);
	m_pWnd2 = NULL;
	UpdateSplitter();
}

// if split vertically, change pane width only
// if split horizontally, change pane height only
void CMailNewsSplitter::SetPaneSize(CWnd *pWnd, int nSize)
{
	ASSERT((m_pWnd1 == pWnd) || (m_pWnd2 == pWnd));

	RECT rect;
	GetClientRect(&rect);

	if (m_bVertical)
	{
		if (nSize > (rect.right - m_nSliderWidth)) 
		{
			if (m_pWnd1 == pWnd)
				m_rcSlider.left = rect.right - m_nSliderWidth;
			else if (m_pWnd2 == pWnd)
				m_rcSlider.left = rect.left;
		}
		else 
		{
			if (m_pWnd1 == pWnd)
				m_rcSlider.left = nSize;
			else if (m_pWnd2 == pWnd)
				m_rcSlider.left = rect.right - nSize;
		}
		m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;				
	}
	else
	{
		if (nSize > (rect.bottom - m_nSliderWidth)) 
		{
			if (m_pWnd1 == pWnd)
				m_rcSlider.top = rect.bottom - m_nSliderWidth;
			else if (m_pWnd2 == pWnd)
				m_rcSlider.top = rect.top;
		}
		else 
		{
			if (m_pWnd1 == pWnd)
				m_rcSlider.top = nSize;
			else if (m_pWnd2 == pWnd)
				m_rcSlider.top = rect.bottom - nSize;
		}
		m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;				
	}
}

// always return width of m_pWnd1 if split vertically
//               height of m_pWnd1 if split horizontally
int CMailNewsSplitter::GetPaneSize()
{
	if (m_bVertical)
		return m_rcSlider.left;
	else
		return m_rcSlider.top;
}

void CMailNewsSplitter::UpdateSplitter()
{
	RECT rect;
	GetClientRect(&rect);
	SetSliderRect(rect.right, rect.bottom);
	PositionWindows(rect.right, rect.bottom);
}

BOOL CMailNewsSplitter::IsInZapper(POINT point)
{
	RECT rect = m_rcSlider;

	if (m_bVertical)
	{
		rect.top = (m_rcSlider.bottom - m_rcSlider.top - ZAP_HEIGHT) / 2;
		rect.bottom = rect.top + ZAP_HEIGHT;
	}
	else
	{
		rect.left = (m_rcSlider.right - m_rcSlider.left - ZAP_HEIGHT) / 2;
		rect.right = rect.left + ZAP_HEIGHT;
	}
	return ::PtInRect(&rect, point);
}

// reset the size and position of the panes and slider 
void CMailNewsSplitter::PositionWindows(int cx, int cy)
{
	if (!cx && !cy)
		return;

	if (!m_pWnd2)
	{
		if (m_pWnd1)
			m_pWnd1->MoveWindow(0, 0, cx, cy, TRUE);
	}
	else
	{
		ASSERT(m_pWnd1);
		ASSERT(m_pWnd2);
		if (m_bVertical)
		{
			if (m_pWnd1)
				m_pWnd1->MoveWindow(0, 0, m_rcSlider.left, cy, TRUE);
			if (m_pWnd2) 
				m_pWnd2->MoveWindow(m_rcSlider.right, 0, 
									cx - m_rcSlider.right, cy, TRUE);
		}
		else
		{
			if (m_pWnd1)
				m_pWnd1->MoveWindow(0, 0, cx, m_rcSlider.top, TRUE);
			if (m_pWnd2) 
				m_pWnd2->MoveWindow(0, m_rcSlider.bottom, 
									cx, cy - m_rcSlider.bottom, TRUE);
		}
	}
	Invalidate();
	UpdateWindow();
}

// initialize and set the slider rect when frame window resize or
// when adding or removing pane from splitter 
// cx - splitter width, cy - splitter height
void CMailNewsSplitter::SetSliderRect(int cx, int cy)
{
	if (m_pWnd1 && m_pWnd2)
	{
		if (m_bVertical)
		{	
			if (::IsRectEmpty(&m_rcSlider))
			{
				if (m_nPaneSize == -1)
				{
					m_rcSlider.left = (cx - m_nSliderWidth) / 2;
					m_nPaneSize = m_rcSlider.left;
				}
				else
					m_rcSlider.left = m_nPaneSize;
			}
			else
			{
				if (m_bZapped)
					m_rcSlider.left = SLIDER_MARGIN;
			}
			m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;
			m_rcSlider.top = 0;
			m_rcSlider.bottom = cy;
		}
		else
		{
			if  (IsRectEmpty(&m_rcSlider))
			{
				if (m_nPaneSize == -1)
				{
					m_rcSlider.top = (cy - m_nSliderWidth) / 2;
					m_nPaneSize = m_rcSlider.top;
				}
				else
					m_rcSlider.top = m_nPaneSize;
			}
			else
			{
				if (m_bZapped)
					m_rcSlider.top = cy - m_nSliderWidth - SLIDER_MARGIN;
			}
			m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;
			m_rcSlider.left = 0;
			m_rcSlider.right = cx;
		}
	}
	else if (!m_pWnd2)
	{
		SetRectEmpty(&m_rcSlider);
	}
}

// Draw the ghost frame for slider when dragging the slider
void CMailNewsSplitter::InvertSlider(RECT* pRect)
{
	ASSERT_VALID(this);
	ASSERT(!IsRectEmpty(pRect));
	ASSERT((GetStyle() & WS_CLIPCHILDREN) == 0);

	HBRUSH hOldBrush = NULL;
	HDC hDC = ::GetDC(GetSafeHwnd());
	if (m_hSliderBrush != NULL)
		hOldBrush = (HBRUSH)::SelectObject(hDC, m_hSliderBrush);
	::PatBlt(hDC, pRect->left, pRect->top, pRect->right - pRect->left, 
				pRect->bottom - pRect->top, PATINVERT);
	if (hOldBrush != NULL)
		SelectObject(hDC, hOldBrush);
	::ReleaseDC(GetSafeHwnd(), hDC);
}

BEGIN_MESSAGE_MAP(CMailNewsSplitter, CView)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SETCURSOR()
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

int CMailNewsSplitter::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int res = CView::OnCreate(lpCreateStruct);

	//create invert slider brush 
	HDC hDC = ::GetDC(GetSafeHwnd());
	WORD grayBits[8]; 
	for (int i = 0; i < 8; i++)
		grayBits[i] = (WORD)(0x5555 << (i & 1));
	HBITMAP grayBitmap = CreateBitmap(8, 8, 1, 1, &grayBits);
	if (grayBitmap != NULL)
	{
		m_hSliderBrush = ::CreatePatternBrush(grayBitmap);
		DeleteObject(grayBitmap);
	}

	CreateBitmaps(hDC);

	::ReleaseDC(GetSafeHwnd(), hDC);

	return res;
}

BOOL CMailNewsSplitter::PreTranslateMessage( MSG* pMsg )
{
	if ( pMsg->message == WM_MOUSEMOVE )
	{
		if ((GetCapture() != this) && m_bZapperDown)
		{
			m_bZapperDown  = FALSE;
			Invalidate();
			UpdateWindow();
		}
	}
	return CView::PreTranslateMessage( pMsg );
}

void CMailNewsSplitter::OnInitialUpdate()
{
	RECT rect;
	GetClientRect(&rect);
	if (m_bVertical && m_nPaneSize <= (rect.left + SLIDER_MARGIN + m_nSliderWidth)) 
		m_bZapped = TRUE;
	else if ((!m_bVertical) && m_nPaneSize >= (rect.bottom - SLIDER_MARGIN - m_nSliderWidth)) 
			m_bZapped = TRUE;
}

void CMailNewsSplitter::OnDraw(CDC *pDC)
{
	HDC hdc = pDC->m_hDC;

	// Fill in background
	HBRUSH hbrushButton = CreateSolidBrush(GetSysColor( COLOR_BTNFACE));
	::FillRect(hdc, &m_rcSlider, hbrushButton);
	VERIFY(::DeleteObject(hbrushButton));

	// draw close pane widge
	HPALETTE hOldPal = ::SelectPalette(pDC->m_hDC, WFE_GetUIPalette(GetParentFrame()), FALSE);
	CDC * pBmpDC = new CDC;
	pBmpDC->CreateCompatibleDC(pDC);
	RECT rect = m_rcSlider;

	HBITMAP hCurrentBmp;
	if (m_bVertical)
	{
		if (m_bZapped)
		{
			hCurrentBmp = m_bZapperDown ? m_hVShowHBmp: m_hVShowNBmp;
		}
		else
		{
			hCurrentBmp = m_bZapperDown ? m_hVCloseHBmp: m_hVCloseNBmp;
		}
	}
	else
	{
		if (m_bZapped)
		{
			hCurrentBmp = m_bZapperDown ? m_hHShowHBmp: m_hHShowNBmp;
		}
		else
		{
			hCurrentBmp = m_bZapperDown ? m_hHCloseHBmp: m_hHCloseNBmp;
		}
	}
	HBITMAP hOldBmp = (HBITMAP)::SelectObject(pBmpDC->m_hDC ,hCurrentBmp);
	HPALETTE hOldPalette = ::SelectPalette(pBmpDC->m_hDC, WFE_GetUIPalette(NULL), TRUE);
	::RealizePalette(pBmpDC->m_hDC);

	if (m_bVertical)
		::BitBlt(pDC->m_hDC, m_rcSlider.left, 
			(m_rcSlider.bottom - m_rcSlider.top - ZAP_HEIGHT) / 2,
			SLIDER_PIXELS, ZAP_HEIGHT, pBmpDC->m_hDC, 0, 0, SRCCOPY);
	else
		::BitBlt(pDC->m_hDC, (m_rcSlider.right - m_rcSlider.left - ZAP_HEIGHT) / 2, 
			m_rcSlider.top, ZAP_HEIGHT, SLIDER_PIXELS, pBmpDC->m_hDC, 
			0, 0, SRCCOPY);

	// Cleanup
	::SelectObject(pBmpDC->m_hDC, hOldBmp);
	::SelectPalette(pBmpDC->m_hDC, hOldPalette, TRUE);
	::SelectPalette(pDC->m_hDC, hOldPal, TRUE);
	pBmpDC->DeleteDC();
	delete pBmpDC;
}

void CMailNewsSplitter::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_bTrackSlider = ::PtInRect(&m_rcSlider, point))
	{
		SetCapture();

		if (IsInZapper(point))
		{
			RECT rect = m_rcSlider;

			m_bZapperDown = TRUE;
			if (m_bVertical)
			{
				rect.top = (m_rcSlider.bottom - m_rcSlider.top - ZAP_HEIGHT) / 2;
				rect.bottom = rect.top + ZAP_HEIGHT;
			}
			else
			{
				rect.left = (m_rcSlider.right - m_rcSlider.left - ZAP_HEIGHT) / 2;
				rect.right = rect.left + ZAP_HEIGHT;
			}
			InvalidateRect(&rect, TRUE);
			UpdateWindow();
		}
		m_ptHit = point;
		m_ptFirstHit = point;
		InvertSlider(&m_rcSlider);
	}
}

void CMailNewsSplitter::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bTrackSlider)
	{
		if (IsInZapper(point))
		{
			if (!m_bZapperDown)
			{
				m_bZapperDown = TRUE;
				Invalidate();
				UpdateWindow();
			}
		}
		else
		{
			if (m_bZapperDown)
			{
				m_bZapperDown = FALSE;
				Invalidate();
				UpdateWindow();
			}
		}
	}
	if (GetCapture() == this) 
	{
		RECT rect, oldRect, newRect;
		
		m_bMouseMove = TRUE;
		if (m_bTrackSlider)
		{
			GetClientRect(&rect);
			oldRect = m_rcSlider;
			if (m_bVertical)
			{
				if (!m_bZapped && point.x < (rect.left + m_nSliderWidth + ZAP_MARGIN)) 
				{
					ReleaseCapture();
					m_bZapped = TRUE;
					m_bTrackSlider = FALSE;
					m_rcSlider.left = rect.left + SLIDER_MARGIN;
					m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;				
					if (m_pWnd1)
						m_pWnd1->MoveWindow(0, 0, m_rcSlider.left, 
										rect.bottom - rect.top, TRUE);
					if (m_pWnd2) 
						m_pWnd2->MoveWindow(m_rcSlider.right, 0, rect.right  - m_rcSlider.right, 
											rect.bottom - rect.top, TRUE);
					Invalidate();
					UpdateWindow();
					return;
				}
				else if (m_bZapped && point.x < (rect.left + m_nSliderWidth)) 
					m_rcSlider.left = rect.left + SLIDER_MARGIN;
				else if (point.x > (rect.right - m_nSliderWidth)) 
					m_rcSlider.left = rect.right - m_nSliderWidth - SLIDER_MARGIN;
				else
					m_rcSlider.left += (point.x - m_ptHit.x); 					
				m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;				
			}
			else
			{
				if (point.y < (rect.top + m_nSliderWidth)) 
					m_rcSlider.top = rect.top + SLIDER_MARGIN;
				else if (!m_bZapped && point.y > (rect.bottom - m_nSliderWidth - ZAP_MARGIN)) 
				{
					ReleaseCapture();
					m_bZapped = TRUE;
					m_bTrackSlider = FALSE;
					m_rcSlider.top = rect.bottom - m_nSliderWidth - SLIDER_MARGIN;
					m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;				
					if (m_pWnd1)
						m_pWnd1->MoveWindow(0, 0, rect.right - rect.left, 
											m_rcSlider.top, TRUE);
					if (m_pWnd2) 
						m_pWnd2->MoveWindow(0, m_rcSlider.bottom, rect.right - rect.left, 
											rect.bottom - m_rcSlider.bottom, TRUE);
					Invalidate();
					UpdateWindow();
					return;
				}
				else if (m_bZapped && point.y > (rect.bottom - m_nSliderWidth)) 
					m_rcSlider.top = rect.bottom - m_nSliderWidth - SLIDER_MARGIN;
				else
					m_rcSlider.top += (point.y - m_ptHit.y); 					
				m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;				
			}
			newRect =  m_rcSlider;

			InvertSlider(&oldRect);		
			InvertSlider(&newRect);	
			m_ptHit = point;
		}
	}
}

void CMailNewsSplitter::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDoubleClicked)
	{
		if (GetCapture() == this) 
			ReleaseCapture();
		m_bDoubleClicked = FALSE;
		m_bTrackSlider = FALSE;
		m_bMouseMove = FALSE;
		m_bZapperDown = FALSE;
		return;
	}

	if (GetCapture() == this) 
	{
		ReleaseCapture();

		RECT rect;
		GetClientRect(&rect);

		if (m_bVertical)
		{
			if (IsInZapper(point) && ((!m_bMouseMove) ||
				(m_bMouseMove && ((abs(point.x - m_ptFirstHit.x) < 5) ||
				 abs(point.x - m_ptFirstHit.x) < 5 && abs(point.y - m_ptFirstHit.y) < 5))))
			{
				if (m_bZapped)
				{
					if (m_nPrevSize > 0)
						m_rcSlider.left = m_nPrevSize;
					else
						m_rcSlider.left = (rect.right - rect.left) / 2;
				}
				else
				{
					m_nPrevSize	= m_rcSlider.left;
					m_rcSlider.left = rect.left + SLIDER_MARGIN;
				}
				m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;
				m_bZapped = !m_bZapped;
			}
			else if (m_bTrackSlider)
			{
				m_rcSlider.left += (point.x - m_ptHit.x); 					
				m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;
				if (m_bZapped)
					m_bZapped = FALSE;
			}
			if (m_pWnd1)
				m_pWnd1->MoveWindow(0, 0, m_rcSlider.left, 
								rect.bottom - rect.top, TRUE);
			if (m_pWnd2) 
				m_pWnd2->MoveWindow(m_rcSlider.right, 0, rect.right  - m_rcSlider.right, 
									rect.bottom - rect.top, TRUE);
		}
		else
		{
			if (IsInZapper(point) && ((!m_bMouseMove) ||
				(m_bMouseMove && ((abs(point.y - m_ptFirstHit.y) < 5) || 
				 abs(point.y - m_ptFirstHit.y) < 5 && abs(point.x - m_ptFirstHit.x) < 5))))
			{
				if (m_bZapped)
				{
					if (m_nPrevSize > 0)
					{
						m_rcSlider.top = m_nPrevSize;
						if (m_rcSlider.top > rect.bottom)
							m_rcSlider.top = rect.bottom - ZAP_MARGIN * 2;
					}
					else
						m_rcSlider.top = (rect.bottom - rect.top) / 2;
				}
				else
				{
					m_nPrevSize	= m_rcSlider.top;
					m_rcSlider.top = rect.bottom - m_nSliderWidth - SLIDER_MARGIN;
				}
				m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;				
				m_bZapped = !m_bZapped;
			}
			else if (m_bTrackSlider)
			{
				m_rcSlider.top += (point.y - m_ptHit.y); 
				m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;
				if (m_bZapped)
					m_bZapped = FALSE;
			}
			if (m_pWnd1)
				m_pWnd1->MoveWindow(0, 0, rect.right - rect.left, 
									m_rcSlider.top, TRUE);
			if (m_pWnd2) 
				m_pWnd2->MoveWindow(0, m_rcSlider.bottom, rect.right - rect.left, 
									rect.bottom - m_rcSlider.bottom, TRUE);
		}
		m_bTrackSlider = FALSE;
		m_bMouseMove = FALSE;
		m_bZapperDown = FALSE;
		Invalidate();
		UpdateWindow();
	}
}

void CMailNewsSplitter::OnLButtonDblClk( UINT nFlags, CPoint point )
{
	if (PtInRect(&m_rcSlider, point) && !IsInZapper(point))
	{
		m_bTrackSlider = FALSE;
		m_bDoubleClicked = TRUE;
//		if (GetCapture() == this) 
//			ReleaseCapture();
		RECT rect;
		GetClientRect(&rect);
		if (m_bVertical)
		{
			if (m_bZapped)
			{
				if (m_nPrevSize > 0)
					m_rcSlider.left = m_nPrevSize;
				else
					m_rcSlider.left = (rect.right - rect.left) / 2;
			}
			else
			{
				m_nPrevSize	= m_rcSlider.left;
				m_rcSlider.left = rect.left + SLIDER_MARGIN;
			}
			m_rcSlider.right = m_rcSlider.left + m_nSliderWidth;
			m_bZapped = !m_bZapped;
			if (m_pWnd1)
				m_pWnd1->MoveWindow(0, 0, m_rcSlider.left, 
								rect.bottom - rect.top, TRUE);
			if (m_pWnd2) 
				m_pWnd2->MoveWindow(m_rcSlider.right, 0, rect.right  - m_rcSlider.right, 
									rect.bottom - rect.top, TRUE);
		}
		else
		{
			if (m_bZapped)
			{
				if (m_nPrevSize > 0)
					m_rcSlider.top = m_nPrevSize;
				else
					m_rcSlider.top = (rect.bottom - rect.top) / 2;
			}
			else
			{
				m_nPrevSize	= m_rcSlider.top;
				m_rcSlider.top = rect.bottom - m_nSliderWidth - SLIDER_MARGIN;
			}
			m_rcSlider.bottom = m_rcSlider.top + m_nSliderWidth;				
			m_bZapped = !m_bZapped;
			if (m_pWnd1)
				m_pWnd1->MoveWindow(0, 0, rect.right - rect.left, 
									m_rcSlider.top, TRUE);
			if (m_pWnd2) 
				m_pWnd2->MoveWindow(0, m_rcSlider.bottom, rect.right - rect.left, 
									rect.bottom - m_rcSlider.bottom, TRUE);
		}
		Invalidate();
		UpdateWindow();
	}
}

BOOL CMailNewsSplitter::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (nHitTest == HTCLIENT) 
	{
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(&point);

		if (::PtInRect(&m_rcSlider, point))  
		{
			RECT rect = m_rcSlider;

			if (IsInZapper(point))
				return CView::OnSetCursor(pWnd, nHitTest, message);
			//	SetCursor(theApp.LoadCursor(IDC_ACTIVATE_EMBED));
			else if (m_bVertical)
				SetCursor(theApp.LoadCursor (AFX_IDC_HSPLITBAR));
			else
				SetCursor(theApp.LoadCursor (AFX_IDC_VSPLITBAR));
			return TRUE;
		}
	}
	return CView::OnSetCursor(pWnd, nHitTest, message);
}

void CMailNewsSplitter::OnSize(UINT nType, int cx, int cy)
{
	SetSliderRect(cx, cy);

	if (nType != SIZE_MINIMIZED) 
		PositionWindows( cx, cy );
}

void CMailNewsSplitter::OnSetFocus(CWnd* pOldWnd)
{
	if (m_pWnd1)   
		m_pWnd1->SetFocus();
	else if (m_pWnd2)  
		m_pWnd2->SetFocus();

	CView::OnSetFocus(pOldWnd);
}

void CMailNewsSplitter::OnShowWindow(BOOL bShow, UINT nStatus)
{
	m_bEraseBackground |= bShow;
}

BOOL CMailNewsSplitter::OnEraseBkgnd(CDC* pDC)
{
	if ( m_bEraseBackground ) 
	{
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	}
	return TRUE;
}


BOOL CMailNewsSplitter::IsOnePaneClosed() const
{
	CRect rect;
	if (m_bVertical)
	{
		m_pWnd1->GetClientRect(&rect);
		if (rect.right == 0)
			return TRUE;
		else
			return FALSE;
	}
	else
	{
		m_pWnd2->GetClientRect(&rect);
		if (rect.bottom == 0)
			return TRUE;
		else
			return FALSE;
	}

	return FALSE;
}


int CMailNewsSplitter::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	int nResult;

	nResult = CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);

	if (nResult == MA_NOACTIVATE || nResult == MA_NOACTIVATEANDEAT)
		return nResult;   // frame does not want to activate

	CFrameWnd* pParentFrame = GetParentFrame();
	if (pParentFrame != NULL)
	{
		// eat it if this will cause activation
		if (pParentFrame == pDesktopWnd || pDesktopWnd->IsChild(pParentFrame))
			nResult = CView::OnMouseActivate(pDesktopWnd, nHitTest, message);
	}
	return nResult;
}


