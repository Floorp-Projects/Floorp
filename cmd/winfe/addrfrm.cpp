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

// addrfrm.cpp : implementation file
//

#include "stdafx.h"
#include "addrfrm.h"
#ifdef FEATURE_ADDRPROP
#include "addrprop.h"
#endif
#include "template.h"
#include "xpgetstr.h"
#include "wfemsg.h"
#include "msg_srch.h"
#include "dirprefs.h"
#include "nethelp.h"
#include "xp_help.h"
#include "prefapi.h"
#include "intlwin.h"
#include "srchdlg.h"
#include "intl_csi.h"
#include "mailpriv.h"
#include "mnprefs.h"

//RHP - Need this include for the call in confhook.cpp
#include "confhook.h"

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CAddrFrame, CGenericFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

extern "C" {
#include "xpgetstr.h"
extern int MK_MSG_CANT_CALL_MAILING_LIST;
extern int MK_MSG_CALL_NEEDS_IPADDRESS;
extern int MK_MSG_CALL_NEEDS_EMAILADDRESS;
extern int MK_MSG_CANT_FIND_CALLPOINT;
extern int MK_ADDR_BOOK_CARD;
extern int MK_ADDR_NEW_CARD;
extern int MK_OUT_OF_MEMORY;
extern int	MK_ADDR_FIRST_LAST_SEP;
extern int	MK_ADDR_LAST_FIRST_SEP;
};


#define ADDRESS_BOOK_TIMER          6
#define OUTLINER_TYPEDOWN_TIMER     250
#define TYPEDOWN_SPEED				300
#define LDAP_SEARCH_SPEED			1250
#define OUTLINER_TYPEDOWN_SPEED		1250

static UINT AddrCodes[] = {
    ID_ITEM_ADDUSER,
	ID_ITEM_ADDLIST,
	ID_ITEM_PROPERTIES,
	ID_FILE_NEWMESSAGE,
	ID_LIVE_CALL,
	ID_EDIT_DELETE,
	ID_NAVIGATE_INTERRUPT
    };

int FE_ShowPropertySheetFor (MWContext* context, ABID entryID, PersonEntry* pPerson)
{
	DIR_Server *pab = NULL;
	BOOL result = FALSE;

#ifdef FEATURE_ADDRPROP
	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
	XP_ASSERT (pab);

	if (pab) 
	{
		if (entryID != MSG_MESSAGEIDNONE) 
		{
			CString formattedString;
			char fullname [kMaxFullNameLength];
			AB_GetFullName(pab, theApp.m_pABook, entryID, fullname);
			formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), fullname);
			CAddrEditProperties prop(NULL, pab,
				(const char *) formattedString, ABSTRACTCX(context)->GetDialogOwner(), entryID, pPerson, context);
			prop.SetCurrentPage(0);
			result = prop.DoModal();
		}
		else 
		{
			CString formattedString;
			char* name = NULL;
			name = (char *) XP_ALLOC(kMaxFullNameLength);
			if (!name)
				return MK_OUT_OF_MEMORY;
			XP_STRCPY (name, pPerson->pGivenName);
			if (pPerson->pFamilyName)
			{
				XP_STRNCPY_SAFE (name, XP_GetString (MK_ADDR_FIRST_LAST_SEP), kMaxFullNameLength);
				XP_STRNCPY_SAFE (name, pPerson->pFamilyName, kMaxFullNameLength);
			}

			formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), name);
			XP_FREEIF (name);
			CAddrEditProperties prop(NULL, pab,
				(const char *) formattedString, ABSTRACTCX(context)->GetDialogOwner(), 0, pPerson, context);
			prop.SetCurrentPage(0);
			result = prop.DoModal();
		}
	}

	if (result == IDOK)
		result = TRUE;
	if (result == IDCANCEL)
		result = FALSE;
	// if it is -1 then there was a memory problem creating the dialog 
	// and we should return that
#endif

	return result;
}
  
void CAddrCX::DestroyContext()
{
	CStubsCX::DestroyContext();
}


CAddrBar::CAddrBar()
{
	m_iWidth = 0;
	m_uTypedownClock = 0;
	m_save = "";
	m_savedir = -1;
	m_pFont = NULL;
	m_bRemoveLDAPDir = FALSE;

	PREF_GetBoolPref("mail.addr_book.ldap.disabled", &m_bRemoveLDAPDir);

	//{{AFX_DATA_INIT(CAddrbkView)
	m_name = _T("");
	m_directory = 0;
	//}}AFX_DATA_INIT
	// TODO: add construction code here
}

CAddrBar::~CAddrBar()
{
	if (m_pFont) {
		theApp.ReleaseAppFont(m_pFont);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CAddrBar Message handlers

BEGIN_MESSAGE_MAP(CAddrBar, CDialogBar)
	//{{AFX_MSG_MAP(CAddrFrame)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_TIMER ()
	ON_EN_CHANGE(IDC_ADDRNAME, OnChangeName)
 	ON_EN_SETFOCUS(IDC_ADDRNAME, OnSetFocusName)
	ON_CBN_SELCHANGE(IDC_DIRECTORIES, OnChangeDirectory)
	ON_BN_CLICKED(IDC_DIRSEARCH, OnExtDirectorySearch)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CAddrBar::Create(CWnd* pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID)
{
	BOOL res = CDialogBar::Create(pParentWnd, nIDTemplate, nStyle, nID);

	m_iWidth = m_sizeDefault.cx;

	return res;
}


int CAddrBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
    int retval =  CDialogBar::OnCreate( lpCreateStruct);

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

	CRect rect;
	GetClientRect(&rect);
	m_sizeDefault = rect.Size();

	return retval;
}

void CAddrBar::SetDirectoryIndex( int dirIndex )
{
   	CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
	m_savedir = -1;
	widget->SetCurSel (dirIndex);
	UpdateData(TRUE);
	if (m_directory != m_savedir) {
		m_savedir = m_directory;
		if (m_directory != -1) {
			XP_Bool bSelectable = FALSE, bPlural = FALSE;
			MSG_COMMAND_CHECK_STATE sState;
			CAddrFrame* frame = (CAddrFrame*) GetParentFrame();
			AB_CommandStatus((ABPane*) frame->GetPane(),
							  AB_LDAPSearchCmd,
							  NULL, NULL,
							  &bSelectable,
							  &sState,
							  NULL,
							  &bPlural);
			CWnd* widget = GetDlgItem ( IDC_DIRSEARCH );
			widget->EnableWindow(bSelectable);
		}
	}
}


void CAddrBar::UpdateDirectories()
{
	DIR_Server* dir;
	CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);

	widget->ResetContent();

	XP_List *pTraverse = theApp.m_directories;
	while (dir = (DIR_Server*) XP_ListNextObject(pTraverse)) {
		if (m_bRemoveLDAPDir){
			if (dir->dirType == PABDirectory)
				widget->AddString(dir->description);
		}
		else if (dir->dirType == PABDirectory || dir->dirType == LDAPDirectory)
			widget->AddString(dir->description);
	}

	if (m_directory == -1)
		widget->SetCurSel(0);
	else
		widget->SetCurSel(m_directory);
}

void CAddrBar::OnTimer(UINT nID)
{
	CWnd::OnTimer(nID);
	if (nID == ADDRESS_BOOK_TIMER) {
		KillTimer(m_uTypedownClock);
		CAddrFrame* frame = (CAddrFrame*) GetParentFrame();
		if (!m_name.IsEmpty()) {
			frame->OnTypedown(m_name.GetBuffer(0));
			m_name.ReleaseBuffer();
		}
	}
}

void CAddrBar::OnKillFocus( CWnd* pNewWnd )
{
	KillTimer(m_uTypedownClock);
}

void CAddrBar::DoDataExchange(CDataExchange* pDX)
{
	CDialogBar::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddrbkView)
	DDX_Text(pDX, IDC_ADDRNAME, m_name);
	DDX_CBIndex(pDX, IDC_DIRECTORIES, m_directory);
	//}}AFX_DATA_MAP
}


void CAddrBar::OnChangeName() 
{
	// TODO: Add your control notification handler code here
	CAddrFrame* frame = (CAddrFrame*) GetParentFrame();
	if (frame->IsSearching()) {
		frame->OnExtendedDirectorySearch();
	}

	UpdateData(TRUE);
	if (m_name != m_save) {
		m_save = m_name;
		KillTimer(m_uTypedownClock);
		if (m_name.GetLength())
		{
			DIR_Server* dir;
			CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
			dir = (DIR_Server*) XP_ListGetObjectNum (theApp.m_directories, widget->GetCurSel() + 1);
			if (dir->dirType == PABDirectory)
				m_uTypedownClock = SetTimer(ADDRESS_BOOK_TIMER, TYPEDOWN_SPEED, NULL);
			else {
				int32 prefInt = LDAP_SEARCH_SPEED;
				PREF_GetIntPref("ldap_1.autoComplete.interval", &prefInt);
				m_uTypedownClock = SetTimer(ADDRESS_BOOK_TIMER, prefInt, NULL);
			}
		}
	}
}


void CAddrBar::OnSetFocusName() 
{
	CEdit *widget = (CEdit*) GetDlgItem(IDC_ADDRNAME);
	widget->SetSel(0, -1, TRUE);
}


void CAddrBar::OnChangeDirectory() 
{
	// TODO: Add your control notification handler code here
	CAddrFrame* frame = (CAddrFrame*) GetParentFrame();
	if (frame->IsSearching())
		return;

	UpdateData(TRUE);
	if (m_directory != m_savedir) {
		m_savedir = m_directory;
		if (m_directory != -1) {

			frame->OnChangeDirectory(m_directory);
			XP_Bool bSelectable = FALSE, bPlural = FALSE;
			MSG_COMMAND_CHECK_STATE sState;
			AB_CommandStatus((ABPane*) frame->GetPane(),
							  AB_LDAPSearchCmd,
							  NULL, NULL,
							  &bSelectable,
							  &sState,
							  NULL,
							  &bPlural);
			CWnd* widget = GetDlgItem ( IDC_DIRSEARCH );
			widget->EnableWindow(bSelectable);
		}
	}
}


void CAddrBar::OnExtDirectorySearch() 
{
	// TODO: Add your control notification handler code here	
	CAddrFrame* frame = (CAddrFrame*) GetParentFrame();
	if (m_directory != -1) {
		KillTimer(m_uTypedownClock);
		frame->OnExtendedDirectorySearch();
	}
} 


static void GrowWindow( CWnd *pWnd, int dx, int dy )
{
	CRect rect;
	CWnd *parent;

	pWnd->GetWindowRect(&rect);
	if (parent = pWnd->GetParent())
		parent->ScreenToClient(&rect);

	rect.bottom += dy;
	if (rect.right + dx > 0)
		rect.right += dx;

	pWnd->MoveWindow(&rect);
}


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


void CAddrBar::OnSize( UINT nType, int cx, int cy )
{
	CDialogBar::OnSize( nType, cx, cy );
	if ( cx && m_iWidth && ( cx != m_iWidth ) ) {
		CWnd *widget;
		CRect rect1 (0,0,0,0);
		int dx = cx - m_iWidth;

		widget = GetDlgItem(IDC_DIRSEARCH);
		if (widget) {
			SlideWindow(widget, dx, 0);
			widget->GetWindowRect(&rect1);
		}

		widget = GetDlgItem(IDC_ADDRNAME);
		if (widget) {
			CWnd *parent;
			CRect rect;

			widget->GetWindowRect(&rect);
			if (parent = widget->GetParent()) {
				parent->ScreenToClient(&rect1);
				parent->ScreenToClient(&rect);
				rect.right = rect1.left - 8;
			}
			widget->MoveWindow(&rect);
		}
		m_iWidth = cx;
	}
}

class CAddrFrame;

/////////////////////////////////////////////////////////////////////////////
// CAddrEntryList

class CAddrEntryList: public IMsgList {

	CAddrFrame *m_pAddrFrame;
	unsigned long m_ulRefCount;

public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

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
								   MSG_FolderInfo *folderInfo) {}
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}

	CAddrEntryList( CAddrFrame *pAddrFrame ) {
		m_ulRefCount = 0;
		m_pAddrFrame = pAddrFrame;
	}
};


STDMETHODIMP CAddrEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) m_pAddrFrame;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CAddrEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CAddrEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CAddrEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pAddrFrame) {
		m_pAddrFrame->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CAddrEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pAddrFrame) {
		m_pAddrFrame->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CAddrEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CAddrEntryList::SelectItem( MSG_Pane* pane, int item )
{
}

/////////////////////////////////////////////////////////////////////////////
// CAddrFrame

CAddrFrame *CAddrFrame::Open()
{
	if (!WFE_MSGCheckWizard())
		return NULL;

	if (!theApp.m_pAddressWindow)	//starts the address book window
	{
		CCreateContext context;
		context.m_pCurrentFrame = NULL;
		context.m_pCurrentDoc = NULL;
		context.m_pNewViewClass = NULL;
		context.m_pNewDocTemplate = NULL;

		theApp.m_pAddressWindow = new CAddrFrame();
		if(!theApp.m_pAddressWindow) return NULL;
		CCreateContext Context;
		Context.m_pCurrentFrame = NULL; //  nothing to base on
		Context.m_pCurrentDoc = NULL;  //  nothing to base on
		Context.m_pNewViewClass = NULL; // nothing to base on
		Context.m_pNewDocTemplate = NULL;   //  nothing to base on
		theApp.m_pAddressWindow->LoadFrame(IDR_ADDRESS_MENU, 
		WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE |WS_CLIPCHILDREN|WS_CLIPSIBLINGS, NULL, &Context);
	} 
	else
	{
		theApp.m_pAddressWindow->ActivateFrame();
	}	
	
	return theApp.m_pAddressWindow; 
}


CAddrFrame::CAddrFrame()
{
	CString msg;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(GetContext());

	m_cxType = AddressCX;

	GetContext()->type = MWContextAddressBook;
	GetContext()->fancyFTP = TRUE;
	GetContext()->fancyNews = TRUE;
	GetContext()->intrupt = FALSE;
	GetContext()->reSize = FALSE;
	INTL_SetCSIWinCSID(csi, CIntlWin::GetSystemLocaleCsid());

	m_pOutliner = NULL;
	m_pOutlinerParent = NULL;
	m_addrBookPane = NULL;
	m_bSearching = FALSE;
	m_iProgress = 0;
	m_iWidth = 0;
	m_pSplitter = NULL;

	CAddrEntryList *pInstance = new CAddrEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );
}


CAddrFrame::~CAddrFrame()
{
	if (m_pIAddrList)
		m_pIAddrList->Release();

	if (m_addrBookPane)
		HandleErrorReturn(AB_CloseAddressBookPane(&m_addrBookPane));

	theApp.m_pAddressWindow = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CAddrFrame Overloaded methods

BOOL CAddrFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.hwndParent = NULL;

	cs.x = cs.y = 0;
	cs.cx = cs.cy = 300; // Arbitrary

	BOOL bRet = CGenericFrame::PreCreateWindow(cs);

#ifdef _WIN32
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
#endif

	return bRet;
}


BOOL CAddrFrame::LoadFrame( UINT nIDResource, DWORD dwDefaultStyle, 
		CWnd* pParentWnd, CCreateContext* pContext)
{
	BOOL ret = CGenericFrame::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext);

	if (ret) {

		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(&wp);

		if (wp.rcNormalPosition.left < 0 || wp.rcNormalPosition.top < 0) {
			SetWindowPos( NULL, 0 , 0, 0, 0, SWP_NOSIZE);
			GetWindowPlacement(&wp);
			SetWindowPos( NULL, abs (wp.rcNormalPosition.left) , abs (wp.rcNormalPosition.top),
				0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
		}
		else
			ShowWindow(SW_SHOW);
	}

	return ret;
}


BOOL CAddrFrame::OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext )
{
	BOOL res = CGenericFrame::OnCreateClient(lpcs, pContext);
	if (res) {

		m_pSplitter = (CMailNewsSplitter *) RUNTIME_CLASS(CMailNewsSplitter)->CreateObject();

        ASSERT(m_pSplitter);

#ifdef _WIN32
		m_pSplitter->CreateEx(0, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								  0,0,0,0, this->m_hWnd, (HMENU)AFX_IDW_PANE_FIRST, pContext );
#else
		m_pSplitter->Create( NULL, NULL,
								 WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								 CRect(0,0,0,0), this, AFX_IDW_PANE_FIRST, pContext ); 
#endif
		        
		m_pOutlinerParent = new CAddrOutlinerParent;
#ifdef _WIN32
		m_pOutlinerParent->CreateEx(WS_EX_CLIENTEDGE, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								  0,0,0,0, m_pSplitter->m_hWnd, (HMENU) IDW_RESULTS_PANE, NULL );
#else
		m_pOutlinerParent->Create( NULL, NULL,
								 WS_BORDER|WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								 CRect(0,0,0,0), m_pSplitter, IDW_RESULTS_PANE );
#endif

		m_pOutliner = (CAddrOutliner *) m_pOutlinerParent->m_pOutliner;
		m_pOutlinerParent->CreateColumns ( );
		m_pOutlinerParent->EnableFocusFrame(TRUE);
		m_pOutliner->SetContext( GetContext() );
		SetMainContext (this);
		SetActiveContext (this);

		m_pDirOutlinerParent = new CDirOutlinerParent;
#ifdef _WIN32
		m_pDirOutlinerParent->CreateEx(WS_EX_CLIENTEDGE, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								  0,0,0,0, m_pSplitter->m_hWnd, (HMENU) IDW_RESULTS_PANE, NULL );
#else
		m_pDirOutlinerParent->Create( NULL, NULL,
								 WS_BORDER|WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								 CRect(0,0,0,0), m_pSplitter, IDW_RESULTS_PANE );
#endif

		m_pDirOutliner = (CDirOutliner *) m_pDirOutlinerParent->m_pOutliner;
		m_pDirOutlinerParent->CreateColumns ( );
		m_pDirOutlinerParent->EnableFocusFrame(TRUE);
		m_pDirOutliner->SetContext( GetContext() );

		int32 prefInt = -1;
		PREF_GetIntPref("mail.addr_book.sliderwidth", &prefInt);
		RECT rect;
		GetClientRect(&rect);
		m_pSplitter->AddPanes(m_pDirOutlinerParent, m_pOutlinerParent, prefInt, TRUE);
	}
	
	return res;
}


BOOL CAddrFrame::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN)
	{
        if (pMsg->wParam == VK_DELETE)
        {
            if (GetFocus() == m_barAddr.GetDlgItem(IDC_ADDRNAME)) {
				return FALSE;
			}
        }

		if (pMsg->wParam == VK_TAB) {
			HWND hwndNext = NULL;
			HWND hwndFocus = ::GetFocus();

			HWND hwndDirOutliner = m_pDirOutliner ? m_pDirOutliner->m_hWnd : NULL;
			HWND hwndOutliner = m_pOutliner ? m_pOutliner->m_hWnd : NULL;
	
			HWND hwndAddrBarFirst = ::GetNextDlgTabItem( m_barAddr.m_hWnd, NULL, FALSE );

			HWND hwndAddrBarLast = ::GetNextDlgTabItem( m_barAddr.m_hWnd, hwndAddrBarFirst, TRUE );

			if ( GetKeyState(VK_SHIFT) & 0x8000 ) {

				// Tab backward
				if ( hwndFocus == hwndAddrBarFirst ) {
					// Handle tabbing into outliner
					hwndNext = hwndOutliner;
				} else if (hwndFocus == hwndOutliner) {
					// Handle tabbing out of outliner into the directory list
					if (m_pSplitter->IsOnePaneClosed())
						hwndNext = hwndAddrBarLast;
					else
						hwndNext = hwndDirOutliner;
				} else if ( hwndFocus == hwndDirOutliner ) {
					// Handle tabbing into the outliner
					hwndNext = hwndAddrBarLast;
				}

			} else {

				// Tab forward
				if (hwndFocus == hwndOutliner) {
					// Handle tabbing out of outliner
					hwndNext = hwndAddrBarFirst;
				} else if ( hwndFocus == hwndAddrBarLast ) {
					// handle tabbing into the directory list
					if (m_pSplitter->IsOnePaneClosed())
						hwndNext = hwndOutliner;
					else
						hwndNext = hwndDirOutliner;
				} else if (hwndFocus == hwndDirOutliner) {
					// Handle tabbing out of the directory list
					hwndNext = hwndOutliner;
				}

			}
			if ( hwndNext ) {
				::SetFocus( hwndNext );
				return TRUE;
			}
		} // if tab
	} // if keydown
	return CGenericFrame::PreTranslateMessage(pMsg);
}


void CAddrFrame::OnHelpMenu()
{
	NetHelp(HELP_ADDRESS_BOOK);
}


void CAddrFrame::Progress(MWContext *pContext, const char *pMessage)
{
	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->SetStatusText(pMessage);
		pIStatusBar->Release();
	}
}


void CAddrFrame::SetProgressBarPercent(MWContext *pContext, int32 lPercent)
{
	lPercent = lPercent < 0 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_iProgress == lPercent ) {
		return;
	}

	m_iProgress = lPercent;
	if (GetChrome()) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetProgress( CASTINT(lPercent) );
			pIStatusBar->Release();
		}
	}
} 


STDMETHODIMP CAddrFrame::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) (LPMSGLIST) m_pIAddrList;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) m_pIAddrList;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CAddrFrame::AddRef(void)
{
	return 0; // Not a real component
}

STDMETHODIMP_(ULONG) CAddrFrame::Release(void)
{
	return 0; // Not a real component
}

// IMailFrame interface
CMailNewsFrame *CAddrFrame::GetMailNewsFrame()
{
	return (CMailNewsFrame *) NULL; 
}

MSG_Pane *CAddrFrame::GetPane()
{
	return (MSG_Pane*) m_addrBookPane;
}

void CAddrFrame::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if (notify == MSG_PaneDirectoriesChanged) {
		// stop an ldap search if we need to 
		if (m_bSearching) 
			OnStopSearch();
		UpdateDirectories();
	}
}


#ifdef FEATURE_ADDRESS_BOOK
#include "addrfrm.i00"
#endif

/////////////////////////////////////////////////////////////////////////////
// CAddrFrame message handlers

BEGIN_MESSAGE_MAP(CAddrFrame, CGenericFrame)
	//{{AFX_MSG_MAP(CAddrFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SHOWWINDOW( )
	ON_WM_DESTROY()
	ON_WM_GETMINMAXINFO()
	ON_UPDATE_COMMAND_UI(ID_VIEW_ABTOOLBAR, OnUpdateViewCommandToolbar)
	ON_COMMAND(ID_VIEW_ABTOOLBAR, OnViewCommandToolbar)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_ADDRESSBOOK, OnUpdateShowAddressBookWindow)
	ON_UPDATE_COMMAND_UI(IDS_SECURITY_STATUS, OnUpdateSecureStatus)
	ON_COMMAND(ID_FILE_IMPORT, OnImportFile)
	ON_UPDATE_COMMAND_UI( ID_FILE_IMPORT, OnUpdateImport )
	ON_COMMAND(ID_FILE_SAVEAS, OnExportFile)
	ON_UPDATE_COMMAND_UI( ID_FILE_SAVEAS, OnUpdateExport )
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_UPDATE_COMMAND_UI( IDC_DIRSEARCH, OnUpdateSearch )
	ON_COMMAND(IDC_DIRSEARCH, OnExtDirectorySearch)
	ON_COMMAND(ID_NAVIGATE_INTERRUPT, OnStopSearch)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_INTERRUPT, OnUpdateStopSearch)
	ON_COMMAND(ID_FILE_NEWMESSAGE, OnComposeMsg)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWMESSAGE, OnUpdateComposeMsg)
	ON_COMMAND(ID_LIVE_CALL, OnCall)
	ON_UPDATE_COMMAND_UI(ID_LIVE_CALL, OnUpdateCall)
	ON_COMMAND(ID_ITEM_ADDUSER, OnAddUser)
	ON_UPDATE_COMMAND_UI(ID_ITEM_ADDUSER, OnUpdateAddUser)
	ON_COMMAND(ID_FILE_NEWAB, OnAddAB)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWAB, OnUpdateAddAB)
	ON_COMMAND(ID_FILE_NEWDIR, OnAddDir)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWDIR, OnUpdateAddDir)
	ON_COMMAND(ID_EDIT_DOMAINS, OnHTMLDomains)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DOMAINS, OnUpdateHTMLDomains)
	ON_COMMAND(ID_ITEM_ADDLIST, OnAddList)
	ON_UPDATE_COMMAND_UI(ID_ITEM_ADDLIST, OnUpdateAddList)
	ON_COMMAND(ID_SWITCH_SORT, OnSwitchSortFirstLast)
	ON_UPDATE_COMMAND_UI(ID_SWITCH_SORT, OnUpdateSwitchSort)
	ON_COMMAND(ID_EDIT_DELETE, OnDeleteItem)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateDeleteItem)
	ON_COMMAND(ID_EDIT_UNDO, OnUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateUndo)
	ON_COMMAND(ID_EDIT_REDO, OnRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateRedo)
	ON_COMMAND(ID_ITEM_PROPERTIES, OnItemProperties)
	ON_UPDATE_COMMAND_UI(ID_ITEM_PROPERTIES, OnUpdateItemProperties)
	ON_COMMAND(ID_SORT_TYPE, OnSortType)
	ON_UPDATE_COMMAND_UI(ID_SORT_TYPE, OnUpdateSortType)
	ON_COMMAND(ID_SORT_NAME, OnSortName)
	ON_UPDATE_COMMAND_UI(ID_SORT_NAME, OnUpdateSortName)
	ON_COMMAND(ID_SORT_NICKNAME, OnSortNickName)
	ON_UPDATE_COMMAND_UI(ID_SORT_NICKNAME, OnUpdateSortNickName)
	ON_COMMAND(ID_SORT_EMAILADDRESS, OnSortEmailAddress)
	ON_UPDATE_COMMAND_UI(ID_SORT_EMAILADDRESS, OnUpdateSortEmailAddress)
	ON_COMMAND(ID_SORT_COMPANY, OnSortCompany)
	ON_UPDATE_COMMAND_UI(ID_SORT_COMPANY, OnUpdateSortCompany)
	ON_COMMAND(ID_SORT_LOCALITY, OnSortLocality)
	ON_UPDATE_COMMAND_UI(ID_SORT_LOCALITY, OnUpdateSortLocality)
	ON_COMMAND(ID_ASCENDING, OnSortAscending)
	ON_UPDATE_COMMAND_UI(ID_ASCENDING, OnUpdateSortAscending)
	ON_COMMAND(ID_DECENDING, OnSortDescending)
	ON_UPDATE_COMMAND_UI(ID_DECENDING, OnUpdateSortDescending)
	ON_COMMAND(ID_CREATE_CARD, OnCreateCard)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CAddrFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	void	WFE_AddNewFrameToFrameList(CGenericFrame * pFrame);
	int res = CGenericFrame::OnCreate(lpCreateStruct);

	unsigned long sortby;
	int32 prefInt;
	PREF_GetIntPref("mail.addr_book.sortby",&prefInt);
	switch (prefInt) {
	case ID_COLADDR_TYPE:
	case ID_COLADDR_COMPANY:
	case ID_COLADDR_LOCALITY:
		sortby = ABFullName;
		break;
	case ID_COLADDR_NAME:
		sortby = ABFullName;
		break;
	case ID_COLADDR_EMAIL:
		sortby = ABEmailAddress;
		break;
	case ID_COLADDR_NICKNAME:
		sortby = ABNickname;
		break;
	default:
		sortby = ABFullName;
		break;
	}

	int result = 0;

	DIR_Server* dir = (DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, 1);

	XP_ASSERT (dir);

	if (!dir)
		return -1;

	XP_Bool prefBool;
	PREF_GetBoolPref("mail.addr_book.sort_ascending",&prefBool);

	HandleErrorReturn((result = AB_CreateAddressBookPane(&m_addrBookPane,
			GetContext(),
			WFE_MSGGetMaster())));

	if (result)
		return -1;

	HandleErrorReturn((result = AB_InitializeAddressBookPane(m_addrBookPane,
								dir,
								theApp.m_pABook,
								sortby,
								prefBool)));
	if (result)
		return -1;

	//I'm hardcoding because I don't want this translated
	GetChrome()->CreateCustomizableToolbar("Address_Book"/*ID_ADDRESS_BOOK*/, 3, TRUE);
	CButtonToolbarWindow *pWindow;
	BOOL bOpen, bShowing;

	int32 nPos;

	//I'm hardcoding because I don't want this translated
	GetChrome()->LoadToolbarConfiguration(ID_ADDRESS_BOOK_TOOLBAR, CString("Address_Book_Toolbar"), nPos, bOpen, bShowing);

	LPNSTOOLBAR pIToolBar = NULL;
	GetChrome()->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
	if ( pIToolBar ) {
		pIToolBar->Create( this, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | CBRS_TOP | WS_BORDER);
		pIToolBar->SetButtons( AddrCodes, sizeof(AddrCodes)/sizeof(UINT) );
		pIToolBar->SetSizes( CSize( 30, 27 ), CSize( 23, 21 ) );
		pIToolBar->LoadBitmap( MAKEINTRESOURCE( IDB_ADDRESSTOOLBARP ) );
		pIToolBar->SetToolbarStyle( theApp.m_pToolbarStyle );
		pWindow = new CButtonToolbarWindow(CWnd::FromHandlePermanent(pIToolBar->GetHWnd()), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
		GetChrome()->GetCustomizableToolbar()->AddNewWindow(ID_ADDRESS_BOOK_TOOLBAR, pWindow,nPos, 50, 37, 0, CString(szLoadString(ID_ADDRESS_BOOK_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
		GetChrome()->ShowToolbar(ID_ADDRESS_BOOK_TOOLBAR, bShowing);
#ifdef XP_WIN16
		GetChrome()->GetCustomizableToolbar()->SetBottomBorder(TRUE);
#endif

		pIToolBar->Release();
	}

	LPNSSTATUSBAR pIStatusBar = NULL;
	GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->Create( this );
		pIStatusBar->Release();
	}


	m_barAddr.Create( this, IDD_ADDRESSBOOK, CBRS_TOP, 1 );

	// Initially size window to only dialog + title bar.
	int BorderX = GetSystemMetrics(SM_CXFRAME);

	CRect rect;
	m_barAddr.GetWindowRect(rect);
#ifdef _WIN32
	CSize size = m_barAddr.CalcFixedLayout(FALSE, FALSE);
#else
	CSize size = m_barAddr.m_sizeDefault;
#endif

	// Figure height of title bar + bottom border
	m_iWidth = size.cx;

	int16 iLeft,iRight,iTop,iBottom;
	PREF_GetRectPref("mail.addr_book_window_rect", &iLeft, &iTop, &iRight, &iBottom);
	int screenY = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int screenX = ::GetSystemMetrics(SM_CXFULLSCREEN);

	if ((iLeft >= 0) && (iTop >= 0) && (iRight < screenX) && (iBottom < screenY))
	{
		if (iRight - iLeft > m_iWidth)
			size.cx = iRight - iLeft;
		size.cy = iBottom - iTop;
		SetWindowPos( NULL, iLeft, iTop, size.cx, size.cy,
				  SWP_HIDEWINDOW);
	}
	else {
		size.cy = 300;
		SetWindowPos( NULL, 0, 0, size.cx, size.cy,
				  SWP_HIDEWINDOW);

	}

	m_barAddr.UpdateDirectories();
	m_pOutliner->SetPane(m_addrBookPane);
	m_pDirOutliner->SetPane(m_addrBookPane);
	MSG_SetFEData( (MSG_Pane*) m_addrBookPane, (void *) m_pIAddrList );

	WFE_AddNewFrameToFrameList(this);

	CFrameWnd::RecalcLayout();

	if (m_barAddr.m_pFont)
		::SendMessage(::GetDlgItem(m_barAddr.GetSafeHwnd(), IDC_ADDRNAME),
					  WM_SETFONT, (WPARAM)m_barAddr.m_pFont, FALSE);
	if (m_barAddr.m_bRemoveLDAPDir)
		m_barAddr.GetDlgItem(IDC_DIRSEARCH)->ShowWindow(SW_HIDE);
	m_barAddr.GetDlgItem(IDC_ADDRNAME)->SetFocus();

	return res;
}

void CAddrFrame::UpdateDirectories ( void )
{
	m_barAddr.SetDirectoryIndex(0);
	m_barAddr.OnChangeDirectory();
    m_barAddr.UpdateDirectories();
	m_pDirOutliner->UpdateCount();
}

void CAddrFrame::OnFileClose ( void )
{
    PostMessage ( WM_CLOSE );
}

void CAddrFrame::OnViewCommandToolbar()
{
	GetChrome()->ShowToolbar(ID_ADDRESS_BOOK_TOOLBAR, !GetChrome()->GetToolbarVisible(ID_ADDRESS_BOOK_TOOLBAR));
}

void CAddrFrame::OnUpdateViewCommandToolbar(CCmdUI *pCmdUI)
{
	BOOL bShow = GetChrome()->GetToolbarVisible(ID_ADDRESS_BOOK_TOOLBAR);
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_VIEW_ABTOOLBAR), CASTUINT(MF_BYCOMMAND | MF_STRING), CASTUINT(ID_VIEW_ABTOOLBAR),
                                    szLoadString(CASTUINT(bShow ? IDS_HIDE_ABTOOLBAR : IDS_SHOW_ABTOOLBAR)) );
    } else {
        pCmdUI->SetCheck(bShow);
    }
}

void CAddrFrame::OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI )
{
	CGenericFrame::OnGetMinMaxInfo( lpMMI );

	if (m_iWidth) {
		lpMMI->ptMinTrackSize.x = m_iWidth;
	}
}


void CAddrFrame::OnDestroy()
{
	// DestroyContext will call Interrupt, but if we wait until after DestroyContext
	// to call MSG_SearchFree, the MWContext will be gone, and we'll be reading freed memory
	if (XP_IsContextBusy (GetContext()))
		XP_InterruptContext (GetContext());

	if (m_addrBookPane)
		MSG_SearchFree ((MSG_Pane*) m_addrBookPane);

 	CGenericFrame::OnDestroy();

	if(!IsDestroyed()) {
		DestroyContext();
	}
}


void CAddrFrame::OnClose()
{
	CRect rect;
    WINDOWPLACEMENT wp;
	int i;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(&wp);

	ASSERT(theApp.m_pAddressWindow == this);
                  
	if ( !IsIconic() && !IsZoomed() )
	{
		GetWindowRect ( &rect );
		PREF_SetRectPref("mail.addr_book_window_rect", 
			(int16) rect.left, (int16) rect.top, (int16) rect.right, (int16) rect.bottom);
	}
    
	PREF_SetIntPref("mail.wfe.addr_book.show_value",(int)wp.showCmd);
	PREF_SetIntPref("mail.addr_book.sliderwidth", m_pSplitter->GetPaneSize());

	// we should prompt if changes aren't saved
	// MW fix
	for (i = 0; i < m_listPropList.GetSize(); i++) {
		((CWnd*) m_listPropList.GetAt(i))->DestroyWindow();
	}

	for (i = 0; i < m_userPropList.GetSize(); i++) {
		((CWnd*) m_userPropList.GetAt(i))->DestroyWindow();
	}

	//I'm hardcoding because I don't want this translated
	GetChrome()->SaveToolbarConfiguration(ID_ADDRESS_BOOK_TOOLBAR, CString("Address_Book_Toolbar"));

	CGenericFrame::OnClose();
}



void CAddrFrame::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
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

void CAddrFrame::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffFinishing( asynchronous, notify,
												    where, num );
		}
	}
}


void CAddrFrame::SetSearchResults(MSG_ViewIndex index, int32 num)
{

	CString csStatus;

	ASSERT(m_pOutliner);
	AB_LDAPSearchResults(m_addrBookPane, index, num);
	if (num > 1) {
		csStatus.Format( szLoadString(IDS_SEARCHHITS), num );
	} else if ( num > 0 ) {
		csStatus.LoadString( IDS_SEARCHONEHIT );
	} else {
		csStatus.LoadString( IDS_SEARCHNOHITS );
	}

	if (GetChrome()) {
		LPNSSTATUSBAR pIStatusBar = NULL;
		GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetStatusText(csStatus);
			pIStatusBar->Release();
		}
	}
}


void CAddrFrame::AllConnectionsComplete( MWContext *pContext )
{
	CStubsCX::AllConnectionsComplete( pContext );

	if (m_bSearching) {

		OnStopSearch();

		int32 total = m_pOutliner->GetTotalLines();
		CString csStatus;
		if ( total > 1 ) {
			csStatus.Format( szLoadString( IDS_SEARCHHITS ), total );
		} else if ( total > 0 ) {
			csStatus.LoadString( IDS_SEARCHONEHIT );
		} else {
			csStatus.LoadString( IDS_SEARCHNOHITS );
			CString s;
			if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
				MessageBox(csStatus, s, MB_OK | MB_APPLMODAL);
		}

		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetStatusText( csStatus );
			pIStatusBar->SetProgress( 0 );
			pIStatusBar->Release();
		}
	
		SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

void CAddrFrame::Close()
{
	if (theApp.m_pAddressWindow) {
		theApp.m_pAddressWindow->PostMessage(WM_CLOSE);
	}       
}

void CAddrFrame::OnImportFile()
{
	HandleErrorReturn(AB_ImportFromFile(m_addrBookPane, GetContext()), this);
}

void CAddrFrame::OnExportFile()
{
	HandleErrorReturn(AB_ExportToFile(m_addrBookPane, GetContext()), this);
}


void CAddrFrame::OnTypedown (char* name)
{
	DIR_Server* dir = GetCurrentDirectoryServer();

	if (dir->dirType == LDAPDirectory)	{
		OnDirectoryList(name);
	}
	else
		m_pOutliner->OnTypedown (name);
}


void CAddrFrame::DoUpdateAddressBook( CCmdUI* pCmdUI, AB_CommandType cmd, BOOL bUseCheck )
{
	if(!pCmdUI)
        return;

    XP_Bool bSelectable = FALSE, bPlural = FALSE;
    MSG_COMMAND_CHECK_STATE sState;

	if (m_addrBookPane) {
		if (m_pOutliner) {
			MSG_ViewIndex *indices;
			int count;
			m_pOutliner->GetSelection(indices, count);
			AB_CommandStatus(m_addrBookPane,
							  cmd,
							  indices, count,
							  &bSelectable,
							  &sState,
							  NULL,
							  &bPlural);
		} else {
			AB_CommandStatus(m_addrBookPane,
							  cmd,
							  NULL, 0,
							  &bSelectable,
							  &sState,
							  NULL,
							  &bPlural);
		}
	}
    pCmdUI->Enable(bSelectable);
    if (bUseCheck)
        pCmdUI->SetCheck(sState == MSG_Checked);
    else
        pCmdUI->SetRadio(sState == MSG_Checked);
}


void CAddrFrame::OnExtDirectorySearch()
{
	m_barAddr.OnExtDirectorySearch();
}

void CAddrFrame::OnUpdateShowAddressBookWindow( CCmdUI *pCmdUI )
{
	pCmdUI->Enable(FALSE);
}

void CAddrFrame::OnUpdateSecureStatus(CCmdUI *pCmdUI)
{
	DIR_Server* dir = GetCurrentDirectoryServer();

	if (dir->isSecure)
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}


void CAddrFrame::OnUpdateSearch( CCmdUI *pCmdUI )
{
	DoUpdateAddressBook(pCmdUI, AB_LDAPSearchCmd, TRUE);
}


void CAddrFrame::OnUpdateImport(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_ImportCmd, TRUE);
}

void CAddrFrame::OnUpdateExport(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SaveCmd, TRUE);
}

void CAddrFrame::OnUpdateSortType(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortByTypeCmd, TRUE);
}

void CAddrFrame::OnUpdateSortName(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortByFullNameCmd, TRUE);
}

void CAddrFrame::OnUpdateSortNickName(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortByNickname, TRUE);
}

void CAddrFrame::OnUpdateSortEmailAddress(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortByEmailAddress, TRUE);
}

void CAddrFrame::OnUpdateSortCompany(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortByCompanyName, TRUE);
}

void CAddrFrame::OnUpdateSortLocality(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortByLocality, TRUE);
}

void CAddrFrame::OnUpdateSortAscending(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortAscending, TRUE);
}

void CAddrFrame::OnUpdateSortDescending(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_SortDescending, TRUE);
}

void CAddrFrame::OnUpdateComposeMsg(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_NewMessageCmd);
}

void CAddrFrame::OnUpdateStopSearch(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_LDAPSearchCmd);
}

void CAddrFrame::OnUpdateAddUser(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_AddUserCmd);
}

void CAddrFrame::OnUpdateAddList(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_AddMailingListCmd);
}

void CAddrFrame::OnUpdateAddAB(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_NewAddressBook);
}

void CAddrFrame::OnUpdateAddDir(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_NewLDAPDirectory);
}

void CAddrFrame::OnUpdateItemProperties(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_PropertiesCmd);
}

void CAddrFrame::OnUpdateCall(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_CallCmd);
}

void CAddrFrame::OnUpdateSwitchSort(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_AddUserCmd);
}

void CAddrFrame::OnUpdateDeleteItem(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_DeleteCmd);
}

void CAddrFrame::OnUpdateUndo(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_UndoCmd);
}

void CAddrFrame::OnUpdateRedo(CCmdUI* pCmdUI)
{
    DoUpdateAddressBook(pCmdUI, AB_RedoCmd);
}

void CAddrFrame::OnSwitchSortFirstLast()
{
	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );

	AB_SetSortByFirstName(theApp.m_pABook, !AB_GetSortByFirstName(theApp.m_pABook));

	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
}


void CAddrFrame::OnAddUser ( void )
{	
#ifdef FEATURE_ADDRPROP
	CAddrEditProperties* prop;
	prop = new CAddrEditProperties (this,
	GetCurrentDirectoryServer(),
	XP_GetString (MK_ADDR_NEW_CARD), this, 0, NULL, GetContext());

	prop->SetCurrentPage(0);
	if (prop->Create(this, WS_SYSMENU | WS_POPUP | WS_CAPTION | WS_VISIBLE,
		WS_EX_DLGMODALFRAME)) {
		m_userPropList.Add (prop);	
	} 
#endif
}

void CAddrFrame::OnAddAB ( void )
{
	HandleErrorReturn(AB_Command (m_addrBookPane, AB_NewAddressBook, 0, 0), this);
}


void CAddrFrame::OnAddDir ( void )
{
#ifdef FEATURE_ADDRPROP
	CString label;
	label.LoadString(IDS_ADD_LDAP_SERVER);
	CAddrLDAPProperties serverProperties(this, GetContext(), NULL, label);
	if (IDOK == serverProperties.DoModal())
	{
		DIR_Server* pServer = &serverProperties.m_serverInfo;
		DIR_Server *pNewServer = (DIR_Server *) XP_ALLOC(sizeof(DIR_Server));
		if (pNewServer)
		{
			DIR_InitServer(pNewServer);
			DIR_CopyServer(pServer, &pNewServer);
			if (pNewServer->dirType == LDAPDirectory) {
				DIR_SetServerFileName(pNewServer, pNewServer->serverName);
			}
		}
		XP_ListAddObjectToEnd(theApp.m_directories, pNewServer);
	   	DIR_SaveServerPreferences (theApp.m_directories); 
	}
#endif
}


void CAddrFrame::OnHTMLDomains ( void )
{
	MSG_DisplayHTMLDomainsDialog(GetContext());
}

void CAddrFrame::OnUpdateHTMLDomains( CCmdUI *pCmdUI )
{
	pCmdUI->Enable( TRUE );
}

void CAddrFrame::OnCreateCard ( void )
{
#ifdef FEATURE_ADDRPROP
	CAddrEditProperties* prop;
	char* email = NULL;
	char* name = NULL;
	DIR_Server* pab  = NULL;
	PersonEntry person;
	person.Initialize();
	ABID entryID = MSG_MESSAGEIDNONE;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(GetContext());

	if (FE_UsersMailAddress())
		email = XP_STRDUP (FE_UsersMailAddress());
	if (FE_UsersFullName())
		name = XP_STRDUP (FE_UsersFullName());
	person.pGivenName = name;
	person.pEmailAddress = email;
	person.WinCSID = INTL_GetCSIWinCSID(csi);

	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
	XP_ASSERT (pab);

	if (pab) {

		AB_GetEntryIDForPerson(pab,
			theApp.m_pABook, &entryID, &person);

		if (entryID != MSG_MESSAGEIDNONE) {
			// see it it is already opened 
			if (m_userPropList.GetSize()) { 
				// try to find it
				for (int i = 0; i < m_userPropList.GetSize(); i++) {
					if (((CAddrEditProperties*)m_userPropList.GetAt(i))->GetEntryID() == entryID) {
						((CAddrEditProperties*)m_userPropList.GetAt(i))->BringWindowToTop();
						return;
					}
				}			
			}

			// otherwise open the window
			CString formattedString;
			char fullname [kMaxFullNameLength];
			AB_GetFullName(pab, theApp.m_pABook, entryID, fullname);
			formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), fullname);
			prop = new CAddrEditProperties(this,
				pab, (const char *) formattedString, this, entryID, NULL, GetContext());

			prop->SetCurrentPage(0);
			if (prop->Create(this, WS_SYSMENU | WS_POPUP | WS_CAPTION | WS_VISIBLE,
				WS_EX_DLGMODALFRAME)) {
				m_userPropList.Add (prop);	
			}
		}
		else {
			CString formattedString;
			formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), name);
			prop = new CAddrEditProperties (this,
				pab, formattedString, this, 0, &person, GetContext());
			prop->SetCurrentPage(0);
			if (prop->Create(this, WS_SYSMENU | WS_POPUP | WS_CAPTION | WS_VISIBLE,
					WS_EX_DLGMODALFRAME)) {
				m_userPropList.Add (prop);	
			}
		}
	}
	if (email)
		XP_FREE (email);
	if (name)
		XP_FREE (name);
#endif
}


DIR_Server * CAddrFrame::GetCurrentDirectoryServer()
{
	return (DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, m_pOutliner->GetDirectoryIndex() + 1);
}

void CAddrFrame::OnAddList ( void )
{
	CABMLDialog* dialog;

	dialog = new CABMLDialog (GetCurrentDirectoryServer(),
		this, 0, GetContext());

	if (dialog->Create(this)) {
		m_listPropList.Add (dialog);	
	}  
}


void CAddrFrame::OnDeleteItem()
{
#ifdef FEATURE_ADDRPROP
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);
	for (int i = 0; i < count; i++)
	{
		ABID type;
		ABID entryID;
		// check the entry type and add/remove pages
		entryID  = AB_GetEntryIDAt((AddressPane*)m_addrBookPane, indices[i]);

		AB_GetType(GetCurrentDirectoryServer(), theApp.m_pABook, 
			entryID, &type);
		if (m_userPropList.GetSize() && type == ABTypePerson) { 
			// try to find 
			for (int i = 0; i < m_userPropList.GetSize(); i++) {
				if (((CAddrEditProperties*)m_userPropList.GetAt(i))->GetEntryID() == entryID) {
					((CAddrEditProperties*)m_userPropList.GetAt(i))->DestroyWindow();
				}
			}			
		}

		if (m_listPropList.GetSize() && type == ABTypeList) { 
			for (int i = 0; i < m_listPropList.GetSize(); i++) {
				if (((CABMLDialog*)m_listPropList.GetAt(i))->GetEntryID() == entryID) {
					((CABMLDialog*)m_listPropList.GetAt(i))->DestroyWindow();
				}
			}			
		}
	}

	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );

	HandleErrorReturn(AB_Command (m_addrBookPane, AB_DeleteCmd, indices, count), this);
	m_pOutliner->UpdateCount();
	if ((m_pOutliner->GetFocusLine() > m_pOutliner->GetTotalLines() - 1))
		m_pOutliner->SelectItem(m_pOutliner->GetTotalLines() - 1);
	else
		m_pOutliner->SelectItem(m_pOutliner->GetFocusLine());
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
#endif
}


void CAddrFrame::OnUndo()
{
	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );
	HandleErrorReturn(AB_Command (m_addrBookPane, AB_UndoCmd, 0, 0), this);
	m_pOutliner->UpdateCount();
	if ((m_pOutliner->GetFocusLine() > m_pOutliner->GetTotalLines() - 1))
		m_pOutliner->SelectItem(m_pOutliner->GetTotalLines() - 1);
	else
		m_pOutliner->SelectItem(m_pOutliner->GetFocusLine());
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
}


void CAddrFrame::OnRedo()
{
	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );
	HandleErrorReturn(AB_Command (m_addrBookPane, AB_RedoCmd, 0, 0), this);
	m_pOutliner->UpdateCount();
	if ((m_pOutliner->GetFocusLine() > m_pOutliner->GetTotalLines() - 1))
		m_pOutliner->SelectItem(m_pOutliner->GetTotalLines() - 1);
	else
		m_pOutliner->SelectItem(m_pOutliner->GetFocusLine());
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
}

void CAddrFrame::OnComposeMsg()
{
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection(indices, count);
	HandleErrorReturn(AB_Command (m_addrBookPane, AB_NewMessageCmd, indices, count), this);
}


void CAddrFrame::CloseUserProperties (CAddrEditProperties* wnd, ABID entryID) 
{
#ifdef FEATURE_ADDRPROP
	CAddrEditProperties* prop;
	for (int i = 0; i < m_userPropList.GetSize(); i++) {
		prop = (CAddrEditProperties*) (m_userPropList.GetAt(i));
		if (prop->GetEntryID() == entryID && prop == wnd) {
			m_userPropList.RemoveAt(i);
			break;
		}
	}			
#endif
}


void CAddrFrame::CloseListProperties (CABMLDialog* wnd, ABID entryID) 
{
	CABMLDialog* prop;
	for (int i = 0; i < m_listPropList.GetSize(); i++) {
		prop = (CABMLDialog*) (m_listPropList.GetAt(i));
		if (prop->GetEntryID() == entryID && prop == wnd) {
			m_listPropList.RemoveAt(i);
			break;
		}
	}
}


void CAddrFrame::OnItemProperties()
{
#ifdef FEATURE_ADDRPROP
	if (GetCurrentDirectoryServer()->dirType == LDAPDirectory) {
		MSG_ViewIndex *indices;
		int count;
		m_pOutliner->GetSelection(indices, count);
		HandleErrorReturn(AB_Command (m_addrBookPane, AB_PropertiesCmd, indices, count), this);
	}
	else {
		MSG_ViewIndex *indices;
		int count;
		m_pOutliner->GetSelection(indices, count);
		for (int i = 0; i < count; i++) {
			ABID type;
			ABID entryID;
			// check the entry type and add/remove pages
			entryID  = AB_GetEntryIDAt((AddressPane*)m_addrBookPane, indices[i]);

			AB_GetType(GetCurrentDirectoryServer(), theApp.m_pABook, 
				entryID, &type);
			if (m_userPropList.GetSize() && type == ABTypePerson) { 
				// try to find 
				for (int i = 0; i < m_userPropList.GetSize(); i++) {
					if (((CAddrEditProperties*)m_userPropList.GetAt(i))->GetEntryID() == entryID) {
						((CAddrEditProperties*)m_userPropList.GetAt(i))->BringWindowToTop();
						return;
					}
				}			
			}

			if (m_listPropList.GetSize() && type == ABTypeList) { 
				for (int i = 0; i < m_listPropList.GetSize(); i++) {
					if (((CABMLDialog*)m_listPropList.GetAt(i))->GetEntryID() == entryID) {
						((CABMLDialog*)m_listPropList.GetAt(i))->BringWindowToTop();
						return;
					}
				}			
			}

			// otherwise open an new property sheet
			if (type == ABTypePerson) {
				CAddrEditProperties* prop = NULL;
				CString formattedString;
				char fullname [kMaxFullNameLength];
				AB_GetFullName(GetCurrentDirectoryServer(), theApp.m_pABook, entryID, fullname);
				formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), fullname);
				prop = new CAddrEditProperties(this,
				GetCurrentDirectoryServer(),
				(const char *) formattedString, this, entryID, NULL, GetContext());

				prop->SetCurrentPage(0);
				if (prop->Create(this, WS_SYSMENU | WS_POPUP | WS_CAPTION | WS_VISIBLE,
						WS_EX_DLGMODALFRAME)) {
					m_userPropList.Add (prop);	
				}
			}
			else {
				CABMLDialog* dialog;
				dialog = new CABMLDialog (GetCurrentDirectoryServer(),
					this, entryID, GetContext());
				if (dialog->Create(this)) {
					dialog->ShowWindow(SW_SHOW);
					m_listPropList.Add (dialog);	
				}  
			}
		}
	}
#endif
}


void CAddrFrame::OnCall()
{
	ABID type;
	ABID entryID;
	CString ipAddress;
	CString emailAddress;
	short useServer = 0;

	char szCommandLine[_MAX_PATH];

	// check the entry type and add/remove pages
	entryID  = AB_GetEntryIDAt((AddressPane*)m_addrBookPane, m_pOutliner->GetFocusLine());

	AB_GetType(GetCurrentDirectoryServer(), theApp.m_pABook, 
		entryID, &type);

	if (type != ABTypePerson) {
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			MessageBox(XP_GetString (MK_MSG_CANT_CALL_MAILING_LIST), s, MB_OK | MB_APPLMODAL);
		return;
	}

	// check that info exists for this person
	AB_GetCoolAddress (GetCurrentDirectoryServer(),
		theApp.m_pABook, entryID, ipAddress.GetBuffer(kMaxCoolAddress));
	ipAddress.ReleaseBuffer(-1);
	AB_GetEmailAddress (GetCurrentDirectoryServer(),
		theApp.m_pABook, entryID, emailAddress.GetBuffer(kMaxEmailAddressLength));
	emailAddress.ReleaseBuffer(-1);
	AB_GetUseServer (GetCurrentDirectoryServer(),
		theApp.m_pABook, entryID, &useServer);

	if ((useServer == kSpecificDLS || useServer == kHostOrIPAddress) && !ipAddress.GetLength()) {
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			MessageBox(XP_GetString (MK_MSG_CALL_NEEDS_IPADDRESS), s, MB_OK | MB_APPLMODAL);
		OnItemProperties();
		return;
	}

	if ((useServer == kSpecificDLS || useServer == kDefaultDLS) && !emailAddress.GetLength()) {
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			MessageBox(XP_GetString (MK_MSG_CALL_NEEDS_EMAILADDRESS), s, MB_OK | MB_APPLMODAL);
		OnItemProperties();
		return;
	}

	strcpy (szCommandLine, "");

		if (useServer == 0) 
		{
			CString emailFlag = FEU_GetConfAppProfileString(IDS_CONFAPP_EMAIL);

			if (!emailFlag.IsEmpty())
			{
				strcat(szCommandLine, emailFlag);
				strcat(szCommandLine, " ");
				strcat(szCommandLine, emailAddress);
			}
		}
		else {
			if (useServer == 1) 
			{
				CString emailFlag = FEU_GetConfAppProfileString(IDS_CONFAPP_EMAIL);
				if (!emailFlag.IsEmpty())
				{
					strcat(szCommandLine, emailFlag);
					strcat(szCommandLine, " ");
					strcat(szCommandLine, emailAddress);
					strcat(szCommandLine, " ");
				}

				CString serverFlag = FEU_GetConfAppProfileString(IDS_CONFAPP_SERVER);
				if (!serverFlag.IsEmpty())
				{
					strcat(szCommandLine, serverFlag);
					strcat(szCommandLine, " ");
					strcat(szCommandLine, ipAddress);
				}
			}
			else {
				CString directFlag = FEU_GetConfAppProfileString(IDS_CONFAPP_DIRECTIP);

				if (!directFlag.IsEmpty())
				{
					strcat(szCommandLine, directFlag);
					strcat(szCommandLine, " ");
					strcat(szCommandLine, ipAddress);
				}
			}
		}

		// 
		// Something a bit new...this will be here for passing a users name at the
		// end of a command line.
		//
		CString userNameFlag = FEU_GetConfAppProfileString(IDS_CONFAPP_USERNAME);
		
		if (!userNameFlag.IsEmpty())
		{
			CString szFullUserName;
			AB_GetFullName (GetCurrentDirectoryServer(),
				theApp.m_pABook, entryID, szFullUserName.GetBuffer(kMaxFullNameLength));
			strcat(szCommandLine, " ");
			strcat(szCommandLine, userNameFlag);
			strcat(szCommandLine, " ");
			strcat(szCommandLine, szFullUserName);
		}
	//
	//RHP - Launch conference app
	//
	LaunchConfEndpoint(szCommandLine, GetSafeHwnd());

}


void CAddrFrame::OnSortName()
{
	m_pOutlinerParent->ColumnCommand ( ID_COLADDR_NAME );
}

void CAddrFrame::OnSortType()
{
	m_pOutlinerParent->ColumnCommand ( ID_COLADDR_TYPE );
}

void CAddrFrame::OnSortNickName()
{
	m_pOutlinerParent->ColumnCommand ( ID_COLADDR_NICKNAME );
}

void CAddrFrame::OnSortEmailAddress()
{
	m_pOutlinerParent->ColumnCommand ( ID_COLADDR_EMAIL );
}

void CAddrFrame::OnSortCompany()
{
	m_pOutlinerParent->ColumnCommand ( ID_COLADDR_COMPANY );
}

void CAddrFrame::OnSortLocality()
{
	m_pOutlinerParent->ColumnCommand ( ID_COLADDR_LOCALITY );
}

void CAddrFrame::OnSortAscending()
{
	m_pOutlinerParent->ColumnCommand ( m_pOutliner->GetSortBy() );
}

void CAddrFrame::OnSortDescending()
{
	m_pOutlinerParent->ColumnCommand ( m_pOutliner->GetSortBy() );
}

void CAddrFrame::HandleErrorReturn(int XPErrorId, CWnd* parent, int errorid)
{	
	if (errorid) {
		CString s;
		HWND parentWnd = NULL;
		if (parent)
			parentWnd = parent->m_hWnd;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES )) {
			if (XPErrorId)
			::MessageBox(parentWnd, XP_GetString(XPErrorId), s, MB_OK | MB_APPLMODAL);
			else {
				CString s2;
				s2.LoadString (errorid);
				::MessageBox(parentWnd, s2, s, MB_OK | MB_APPLMODAL);
			}
		}
	}
}


void CAddrFrame::OnSave()
{
}


void CAddrFrame::OnChangeDirectory (int dirIndex)
{
	m_pDirOutliner->SetDirectoryIndex(dirIndex);
}

void CAddrFrame::OnShowWindow( BOOL bShow, UINT nStatus )
{
	CFrameWnd::OnShowWindow (bShow, nStatus);
	if (bShow && (nStatus == 0)) {
		m_pOutliner->SetDirectoryIndex(0);
		m_pDirOutliner->SetDirectoryIndex(0);
	}
}


void CAddrFrame::OnUpdateDirectorySelection (int dirIndex)
{
	// the user has selected a different directory in the left pane
	m_barAddr.SetDirectoryIndex(dirIndex);
	m_pOutliner->SetDirectoryIndex(dirIndex);
	HandleErrorReturn(AB_ChangeDirectory(m_addrBookPane, GetCurrentDirectoryServer()), this);
}


void CAddrFrame::OnStopSearch ()
{
	CWnd* widget = m_barAddr.GetDlgItem(IDC_DIRECTORIES);
	CWnd* widget2 = m_barAddr.GetDlgItem (IDC_DIRSEARCH);
	// We've turned into stop button
	HandleErrorReturn(AB_FinishSearch(m_addrBookPane, GetContext()), this);
	m_bSearching = FALSE;
	GetChrome()->StopAnimation();
	widget->EnableWindow(TRUE);
	widget2->EnableWindow(TRUE);
	m_pDirOutliner->EnableWindow(TRUE);

	MSG_ViewIndex *indices = NULL;
	int count = 0;
	m_pOutliner->GetSelection(indices, count);
	if (!count)
		m_pOutliner->SelectItem (0);
	return;
}

void CAddrFrame::OnExtendedDirectorySearch ()
{
	CWnd* widget = m_barAddr.GetDlgItem(IDC_DIRECTORIES);
	CWnd* widget2 = m_barAddr.GetDlgItem (IDC_DIRSEARCH);

	if (m_bSearching) {
		OnStopSearch();
		return;
	}

	CUIntArray buttonLabels;
	buttonLabels.SetSize (4, 1);
	buttonLabels [0] = IDS_ADRSEARCH;
	buttonLabels [1] = IDS_CANCEL_BUTTON;
	buttonLabels [2] = IDS_HELP_BUTTON;
	buttonLabels [3] = IDS_ADVSEARCH;

	CSearchDialog searchDlg ((UINT) IDS_ADRSEARCH, 
		(MSG_Pane*) m_addrBookPane, 
		GetCurrentDirectoryServer(),
		this, 4, BUTTON_RIGHT, &buttonLabels);
	int result = searchDlg.DoModal(); 

	// Build Search
	if (result == IDOK)
	{
		GetChrome()->StartAnimation();
		LPNSSTATUSBAR pIStatusBar = NULL;
		m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->SetStatusText(szLoadString( IDS_SEARCHING ));
			pIStatusBar->Release();
		}

		m_barAddr.GetDlgItem(IDC_ADDRNAME)->SetWindowText("");
		m_bSearching = TRUE;
		widget->EnableWindow(FALSE);
		widget2->EnableWindow(FALSE);
		m_pOutliner->SetTotalLines(0);
		m_pOutliner->SetFocus();
		m_pOutliner->SetFocusLine(0);
		m_pDirOutliner->EnableWindow(FALSE);

		HandleErrorReturn(AB_SearchDirectory(m_addrBookPane, NULL), this);
	}

}

void CAddrFrame::OnDirectoryList (char * name)
{
	CWnd* widget;
	CWnd* widget2;
	CString cs;
	widget = m_barAddr.GetDlgItem(IDC_DIRECTORIES);
	widget2 = m_barAddr.GetDlgItem(IDC_DIRSEARCH);

	// Build Search
	GetChrome()->StartAnimation();
	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->SetStatusText(szLoadString( IDS_SEARCHING ));
		pIStatusBar->Release();
	}
	m_bSearching = TRUE;
	widget->EnableWindow(FALSE);
	widget2->EnableWindow(FALSE);
	m_pDirOutliner->EnableWindow(FALSE);

	HandleErrorReturn(AB_SearchDirectory(m_addrBookPane, name), this);
	m_pOutliner->UpdateCount();
}

//////////////////////////////////////////////////////////////////////////////
// CAddrOutliner
BEGIN_MESSAGE_MAP(CAddrOutliner, CMSelectOutliner)
	//{{AFX_MSG_MAP(CAddrOutliner)
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrOutliner::CAddrOutliner ( )
{
	int32 prefInt;
	PREF_GetIntPref("mail.addr_book.sortby",&prefInt);
	if (prefInt != ID_COLADDR_NAME && prefInt != ID_COLADDR_EMAIL
		&& prefInt != ID_COLADDR_NICKNAME)
		prefInt = ID_COLADDR_NAME;

	m_attribSortBy = CASTINT(prefInt);
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_ADDRESSBOOK,16,16);
    }
	XP_Bool prefBool;
	PREF_GetBoolPref("mail.addr_book.sort_ascending",&prefBool);
	m_bSortAscending = prefBool;
	m_iMysticPlane = 0;
	m_dirIndex = 0;
	m_hFont = NULL;
	m_psTypedown = "";
	m_uTypedownClock = 0;
}

CAddrOutliner::~CAddrOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }
	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}

}

void CAddrOutliner::OnKillFocus( CWnd* pNewWnd )
{
	CMSelectOutliner::OnKillFocus (pNewWnd );
	m_psTypedown = "";	
	KillTimer(m_uTypedownClock);
}

void CAddrOutliner::OnTypedown (char* name)
{
	MSG_ViewIndex	index;
	int	startIndex;
	
	if (GetFocusLine() != -1)
		startIndex = GetFocusLine();
	else
		startIndex = 0;

	AB_GetIndexMatchingTypedown(m_pane, &index, 
		(LPCTSTR) name, startIndex);

	ScrollIntoView(CASTINT(index));
	SelectItem (CASTINT(index));
}

HFONT CAddrOutliner::GetLineFont(void *pLineData)
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


DIR_Server* CAddrOutliner::GetCurrentDirectoryServer ()
{
	return (DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, m_dirIndex + 1);
}


void CAddrOutliner::SetDirectoryIndex (int dirIndex)
{
	m_dirIndex = dirIndex;
	DIR_Server* pServer = GetCurrentDirectoryServer ();
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
			case ID_COLADDR_LOCALITY:
				MSG_SearchAttribToDirAttrib(attribLocality, &id);				
				text = DIR_GetAttributeName(pServer, id);
				break;
			case ID_COLADDR_PHONE:
				MSG_SearchAttribToDirAttrib(attribPhoneNumber, &id);				
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
	ClearSelection();
	SelectItem(dirIndex);
}

void CAddrOutliner::UpdateCount( )
{
	uint32 count;

	AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
	m_iTotalLines = CASTINT(count);
	if (!m_iTotalLines)
		ClearSelection();
}


BOOL CAddrOutliner::DeleteItem ( int iLine )
{
    XP_Bool bSelectable, bPlural;
	MSG_ViewIndex indices[1];
	indices[0] = (MSG_ViewIndex) iLine;
    MSG_COMMAND_CHECK_STATE sState;
    AB_CommandStatus( m_pane, 
					   AB_DeleteCmd,
					   indices, 1,
					   &bSelectable, 
					   &sState, 
					   NULL,
					   &bPlural);
    if (bSelectable) {
        AB_Command ( m_pane, AB_DeleteCmd, indices, 1 );
        return TRUE;
    }
    else
        MessageBeep(0);
    return FALSE;
}

void CAddrOutliner::SetPane(ABPane *pane)
{
	m_pane = pane;
	uint32 count;

	if (m_pane) {
		AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
		SetTotalLines(CASTINT(count));
		Invalidate();
		UpdateWindow();
	}
}

void CAddrOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CAddrOutliner::MysticStuffFinishing( XP_Bool asynchronous,
											 MSG_NOTIFY_CODE notify, 
											 MSG_ViewIndex where,
											 int32 num )
{
	switch ( notify ) {
	case MSG_NotifyNone:
		break;

	case MSG_NotifyInsertOrDelete:
		// if its insert or delete then tell my frame to add the next chunk of values
		// from the search
		if (notify == MSG_NotifyInsertOrDelete 
			&& ((CAddrFrame*) GetParentFrame())->IsSearching() && num > 0) 
		{
			((CAddrFrame*) GetParentFrame())->SetSearchResults(where, num);
			HandleInsert(where, num);
			// Invalidate();
		}
		else
		{
			if (num > 0)
				HandleInsert(where, num);
			else
				HandleDelete(where, -num);
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

	if (( !--m_iMysticPlane && m_pane)) {
		uint32 count;
		AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
		SetTotalLines(CASTINT(count));
		Invalidate();
		UpdateWindow();
	}
}

void CAddrOutliner::PropertyMenu ( int iSel, UINT flags )	
{
	BOOL bHasItems = FALSE;
	CMenu cmPopup;
	CString cs;
	
	if(cmPopup.CreatePopupMenu() == 0) 
		return;

    if (iSel < m_iTotalLines) {
		cs.LoadString(ID_POPUP_MAILNEW);
		cmPopup.AppendMenu(MF_ENABLED, ID_FILE_NEWMESSAGE, cs);
		cmPopup.AppendMenu(MF_SEPARATOR);
		cs.LoadString(IDS_AB_NEW_USER);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_ADDUSER, cs);
		cs.LoadString(IDS_AB_NEW_LIST);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_ADDLIST, cs);
		cmPopup.AppendMenu(MF_SEPARATOR);
		cs.LoadString(IDS_COMPOSE_REMOVE);
		cmPopup.AppendMenu(MF_ENABLED, ID_EDIT_DELETE, cs);
		cmPopup.AppendMenu(MF_SEPARATOR);
		cs.LoadString(IDS_AB_PROPERTIES);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_PROPERTIES, cs);
		bHasItems = TRUE;
	}


	//	Track the popup now.
	POINT pt = m_ptHit;
	ClientToScreen(&pt);

	cmPopup.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 
						   GetParentFrame(), NULL);

    //  Cleanup
    cmPopup.DestroyMenu();
}

void CAddrOutliner::AcceptDrop( int iLineNo, COleDataObject *pDataObject,
							    DROPEFFECT dropEffect )
{
    if(!pDataObject)
        return;

	// Dont allow us to drop on ourselves
	if(pDataObject->IsDataAvailable(m_cfSourceTarget))
	   return;

	if(!pDataObject->IsDataAvailable(m_cfAddresses) &&
	   !pDataObject->IsDataAvailable(CF_TEXT))
	   return;

		if(pDataObject->IsDataAvailable(m_cfAddresses)) {
			HGLOBAL hAddresses = pDataObject->GetGlobalData(m_cfAddresses);
			LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
			ASSERT(pAddresses);
			// Parse the vcards and add them to the address book
			AB_ImportFromVcard((AddressPane*) m_pane, pAddresses);
			GlobalUnlock(hAddresses);
		}
		else {
			HGLOBAL hString = pDataObject->GetGlobalData(CF_TEXT);
			char * pAddress = (char*)GlobalLock(hString);
			ASSERT(pAddress);
			// parse the vcard from the text
			AB_ImportFromVcard((AddressPane*) m_pane, pAddress);
			GlobalUnlock(hString);
		}
}

void CAddrOutliner::InitializeClipFormats(void)
{
     m_cfAddresses = (CLIPFORMAT)RegisterClipboardFormat(vCardClipboardFormat);
	 m_cfSourceTarget = (CLIPFORMAT)RegisterClipboardFormat(ADDRESSBOOK_SOURCETARGET_FORMAT);
}

CLIPFORMAT * CAddrOutliner::GetClipFormatList(void)
{
    static CLIPFORMAT cfFormatList[3];
    cfFormatList[0] = m_cfAddresses;
	cfFormatList[1] = m_cfSourceTarget;
    cfFormatList[2] = 0;
    return cfFormatList;
}


COleDataSource * CAddrOutliner::GetDataSource(void)
{
    COleDataSource * pDataSource = new COleDataSource;
	ABID	entryID;
	char* pVcard = NULL;
	char* pVcards = XP_STRDUP("");
	char  szTemp[10];
	char  szEntryID[20];
	char* pEntryIDs = XP_STRDUP("");
	HANDLE hString = 0;
	MSG_ViewIndex *indices;
	int count;

	GetSelection(indices, count);

	for (int i = 0 ; i < count; i++) {
		if ((entryID = AB_GetEntryIDAt((AddressPane*) m_pane, indices[i])) != 0) {
			AB_ExportToVCard(theApp.m_pABook, 
				GetCurrentDirectoryServer (), entryID, &pVcard);
			pVcards = StrAllocCat(pVcards, pVcard);
			XP_FREE(pVcard);
			pVcard = NULL;

			ltoa(entryID, szTemp, 10);
			sprintf(szEntryID, "%s-", szTemp);
			pEntryIDs = StrAllocCat(pEntryIDs, szEntryID);
		}
	}

	if (pVcards) {
		hString = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,strlen(pVcards)+1);
        LPSTR lpszString = (LPSTR)GlobalLock(hString);
        strcpy(lpszString,pVcards);
		XP_FREE (pVcards);
        GlobalUnlock(hString);

		HANDLE hEntryID = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,strlen(pEntryIDs)+1);
        LPSTR lpszEntryID = (LPSTR)GlobalLock(hEntryID);
        strcpy(lpszEntryID,pEntryIDs);
		XP_FREE (pEntryIDs);
        GlobalUnlock(hEntryID);
        pDataSource->CacheGlobalData(m_cfAddresses, hString);
		pDataSource->CacheGlobalData(m_cfSourceTarget, hEntryID);
		pDataSource->CacheGlobalData(CF_TEXT, hString);
    }

    return pDataSource;
}


BOOL CAddrOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
	if ( iColumn != ID_COLADDR_TYPE )
        return CMSelectOutliner::RenderData ( iColumn, rect, dc, text );
	int idxImage;

    if (m_EntryLine.entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;

	m_pIUserImage->DrawImage ( idxImage,
		rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
	return TRUE;
}


int CAddrOutliner::TranslateIcon ( void * pLineData )
{
	AB_EntryLine* line = (AB_EntryLine*) pLineData;
	int idxImage = 0;

    if (line->entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;
	return idxImage;
}

int CAddrOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CAddrOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

void * CAddrOutliner::AcquireLineData ( int line )
{
	m_lineindex = line + 1;
	if ( line >= m_iTotalLines)
		return NULL;
	if (!AB_GetEntryLine(m_pane, line, &m_EntryLine ))
        return NULL;

	return &m_EntryLine;
}

void CAddrOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}

void CAddrOutliner::ReleaseLineData ( void * )
{
}

LPCTSTR CAddrOutliner::GetColumnText ( UINT iColumn, void * pLineData )
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

void CAddrOutliner::OnSelChanged()
{
	// No Op
}

void CAddrOutliner::OnSelDblClk()
{
	GetParentFrame()->SendMessage(WM_COMMAND, ID_ITEM_PROPERTIES, 0);
}


void CAddrOutliner::OnTimer(UINT nID)
{
	CMSelectOutliner::OnTimer(nID);
	if (nID == OUTLINER_TYPEDOWN_TIMER) {
		KillTimer(m_uTypedownClock);
		m_psTypedown = "";
	}
}

void CAddrOutliner::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	CMSelectOutliner::OnKeyDown ( nChar, nRepCnt, nFlags );
	KillTimer(m_uTypedownClock);
	if (nChar > VK_HELP) {
		KillTimer(m_uTypedownClock);
		m_psTypedown += nChar;
		OnTypedown (m_psTypedown.GetBuffer (0));
		m_psTypedown.ReleaseBuffer(-1);
		int32 prefInt = OUTLINER_TYPEDOWN_SPEED;
		PREF_GetIntPref("ldap_1.autoComplete.interval", &prefInt);
		m_uTypedownClock = SetTimer(OUTLINER_TYPEDOWN_TIMER, prefInt, NULL);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CAddrOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CAddrOutlinerParent)
	ON_WM_DESTROY()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrOutlinerParent::CAddrOutlinerParent()
{

}


CAddrOutlinerParent::~CAddrOutlinerParent()
{
 
}


BOOL CAddrOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CAddrOutliner *pOutliner = (CAddrOutliner *) m_pOutliner;
	
	// Calculate text offset from top using font height.
    TEXTMETRIC tm;
    dc.GetTextMetrics ( &tm );
    cy = ( rect.bottom - rect.top - tm.tmHeight ) / 2;
        
	// Draw Text String
	dc.TextOut (rect.left + cx, rect.top + cy, text, _tcslen(text) );
    
	// Draw Sort Indicator
	MSG_COMMAND_CHECK_STATE sortType = pOutliner->m_attribSortBy == idColumn ? MSG_Checked : MSG_Unchecked;

	int idxImage = pOutliner->m_bSortAscending ? IDX_SORTINDICATORDOWN : IDX_SORTINDICATORUP;
	CSize cs = dc.GetTextExtent(text, _tcslen(text));

	if (idColumn == pOutliner->m_attribSortBy && cs.cx + 22 <= rect.Width()) {
        m_pIImage->DrawTransImage( idxImage,
								   rect.left + 8 + cs.cx,
								   (rect.top + rect.bottom) / 2 - 4,
								   &dc );
	}

    return TRUE;
}

COutliner * CAddrOutlinerParent::GetOutliner ( void )
{
    return new CAddrOutliner;
}

void CAddrOutlinerParent::CreateColumns ( void )
{
	int iCol0 = 0,
		iCol1 = 0,
	    iCol2 = 0,
        iCol3 = 0,
        iCol4 = 0,
		iCol5 = 0;
	CString cs; 

	m_pOutliner->AddColumn ("",			ID_COLADDR_TYPE,		24, 0, ColumnFixed, 0, TRUE );
	cs.LoadString(IDS_USERNAME);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_NAME,		1500, 0, ColumnVariable, iCol0 ? iCol0 : 1500 );
	cs.LoadString(IDS_EMAILADDRESS);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_EMAIL,		1500, 0, ColumnVariable, iCol2 ? iCol2 : 1500 ); 
	cs.LoadString(IDS_COMPANYNAME);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_COMPANY,		1500, 0, ColumnVariable, iCol5 ? iCol5 : 1500 ); 
	cs.LoadString(IDS_PHONE);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_PHONE,		1500, 0, ColumnVariable, iCol4 ? iCol4 : 1500, FALSE);
	cs.LoadString(IDS_LOCALITY);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_LOCALITY,	1500, 0, ColumnVariable, iCol4 ? iCol4 : 1500 );
	cs.LoadString(IDS_NICKNAME);
    m_pOutliner->AddColumn (cs,			ID_COLADDR_NICKNAME,	1500, 0, ColumnVariable, iCol3 ? iCol3 : 1500 );
	m_pOutliner->SetHasPipes( FALSE );

	m_pOutliner->SetVisibleColumns(DEF_VISIBLE_COLUMNS);
	m_pOutliner->LoadXPPrefs("mailnews.abook_columns_win");
}

void CAddrOutlinerParent::OnDestroy()
{
	int32 prefInt = ((CAddrOutliner*)m_pOutliner)->GetSortBy();
	if (prefInt != ID_COLADDR_NAME && prefInt != ID_COLADDR_EMAIL
		&& prefInt != ID_COLADDR_NICKNAME)
		prefInt = ID_COLADDR_NAME;

	PREF_SetIntPref("mail.addr_book.sortby", prefInt);
	PREF_SetBoolPref("mail.addr_book.sort_ascending",((CAddrOutliner*)m_pOutliner)->GetSortAscending());
	
	COutlinerParent::OnDestroy();
}

void CAddrOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.abook_columns_win");
}

BOOL CAddrOutlinerParent::ColumnCommand ( int idColumn )
{	
	ABID lastSelection;

	CAddrOutliner *pOutliner = (CAddrOutliner *) m_pOutliner;
	
	if (pOutliner->GetFocusLine() != -1)
		lastSelection = AB_GetEntryIDAt((AddressPane*) pOutliner->GetPane(), pOutliner->GetFocusLine());

	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );

	if (idColumn == pOutliner->m_attribSortBy) {
		pOutliner->m_bSortAscending = !pOutliner->m_bSortAscending; 
	}
	else
		pOutliner->m_bSortAscending = TRUE;

	pOutliner->m_attribSortBy = idColumn;

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
		uint32 index = AB_GetIndexOfEntryID ((AddressPane *) pOutliner->GetPane(), lastSelection);
		pOutliner->SelectItem (CASTINT(index));
		pOutliner->ScrollIntoView(CASTINT(index));
	}

	Invalidate();
	pOutliner->Invalidate();
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
	return TRUE;
}




//////////////////////////////////////////////////////////////////////////////
// CAddrOutliner
BEGIN_MESSAGE_MAP(CDirOutliner, CMSelectOutliner)
	//{{AFX_MSG_MAP(CDirOutliner)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CDirOutliner::CDirOutliner ( )
{
	m_attribSortBy = 0;
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_DIRLIST,16,16);
    }
	
	m_attribSortBy = 1;
	m_bSortAscending = TRUE;
	m_iMysticPlane = 0;
	m_dirIndex = 0;
	m_hFont = NULL;
	m_pDirLine = NULL;
}

CDirOutliner::~CDirOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }
	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}

}


HFONT CDirOutliner::GetLineFont(void *pLineData)
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

void CDirOutliner::SetDirectoryIndex (int dirIndex)
{
	m_dirIndex = dirIndex;
	SelectItem (CASTINT(m_dirIndex));
}

void CDirOutliner::UpdateCount( )
{
	m_iTotalLines = XP_ListCount (theApp.m_directories);
	if (!m_iTotalLines)
		ClearSelection();
	Invalidate();
	UpdateWindow();
}


BOOL CDirOutliner::DeleteItem ( int iLine )
{
	MSG_ViewIndex indices[1];
	indices[0] = (MSG_ViewIndex) iLine;
    MessageBeep(0);
    return TRUE;
}

void CDirOutliner::SetPane(ABPane *pane)
{
	m_pane = pane;

	if (m_pane) {
		SetTotalLines(XP_ListCount (theApp.m_directories));
		Invalidate();
		UpdateWindow();
	}
}

void CDirOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CDirOutliner::MysticStuffFinishing( XP_Bool asynchronous,
											 MSG_NOTIFY_CODE notify, 
											 MSG_ViewIndex where,
											 int32 num )
{
	switch ( notify ) {
	case MSG_NotifyNone:
		break;

	case MSG_NotifyInsertOrDelete:
		if (num > 0)
			HandleInsert(where, num);
		else
			HandleDelete(where, -num);
		break;

	case MSG_NotifyChanged:
		InvalidateLines( (int) where, (int) num );
		break;

	case MSG_NotifyAll:
	case MSG_NotifyScramble:
		Invalidate();
		break;
	}

	if (( !--m_iMysticPlane && m_pane)) {
		SetTotalLines(XP_ListCount (theApp.m_directories));
		Invalidate();
		UpdateWindow();
	}
}

void CDirOutliner::PropertyMenu ( int iSel, UINT flags )	
{
	BOOL bHasItems = FALSE;
	CMenu cmPopup;
	CString cs;
	
	if(cmPopup.CreatePopupMenu() == 0) 
		return;

    if (iSel < m_iTotalLines) {
		cs.LoadString(IDS_AB_NEW_USER);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_ADDUSER, cs);
		cs.LoadString(IDS_AB_NEW_LIST);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_ADDLIST, cs);
		cmPopup.AppendMenu(MF_SEPARATOR);
		cs.LoadString(IDS_COMPOSE_REMOVE);
		cmPopup.AppendMenu(MF_ENABLED, ID_EDIT_DELETE, cs);
		cmPopup.AppendMenu(MF_SEPARATOR);
		cs.LoadString(IDS_AB_PROPERTIES);
		cmPopup.AppendMenu(MF_ENABLED, ID_ITEM_PROPERTIES, cs);
		bHasItems = TRUE;
	}


	//	Track the popup now.
	POINT pt = m_ptHit;
	ClientToScreen(&pt);

	cmPopup.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 
						   GetParentFrame(), NULL);

    //  Cleanup
    cmPopup.DestroyMenu();
}

void CDirOutliner::AcceptDrop( int iLineNo, COleDataObject *pDataObject,
							    DROPEFFECT dropEffect )
{
    if(!pDataObject)
        return;

	// Dont allow us to drop on ourselves
	if(pDataObject->IsDataAvailable(m_cfSourceTarget))
	   return;

	if(!pDataObject->IsDataAvailable(m_cfAddresses) &&
	   !pDataObject->IsDataAvailable(CF_TEXT))
	   return;

		if(pDataObject->IsDataAvailable(m_cfAddresses)) {
			HGLOBAL hAddresses = pDataObject->GetGlobalData(m_cfAddresses);
			LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
			ASSERT(pAddresses);
			// Parse the vcards and add them to the address book
			// AB_ImportFromVcard((AddressPane*) m_pane, pAddresses);
			GlobalUnlock(hAddresses);
		}
		else {
			HGLOBAL hString = pDataObject->GetGlobalData(CF_TEXT);
			char * pAddress = (char*)GlobalLock(hString);
			ASSERT(pAddress);
			// parse the vcard from the text
			// AB_ImportFromVcard((AddressPane*) m_pane, pAddress);
			GlobalUnlock(hString);
		}
}

void CDirOutliner::InitializeClipFormats(void)
{
     m_cfAddresses = (CLIPFORMAT)RegisterClipboardFormat(vCardClipboardFormat);
	 m_cfSourceTarget = (CLIPFORMAT)RegisterClipboardFormat(ADDRESSBOOK_SOURCETARGET_FORMAT);
}

CLIPFORMAT * CDirOutliner::GetClipFormatList(void)
{
    static CLIPFORMAT cfFormatList[3];
    cfFormatList[0] = m_cfAddresses;
	cfFormatList[1] = m_cfSourceTarget;
    cfFormatList[2] = 0;
    return cfFormatList;
}


COleDataSource * CDirOutliner::GetDataSource(void)
{
    COleDataSource * pDataSource = new COleDataSource;
	MSG_ViewIndex *indices;
	int count;

	/*
	ABID	entryID;
	char* pVcard = NULL;
	char* pVcards = XP_STRDUP("");
	char  szTemp[10];
	char  szEntryID[20];
	char* pEntryIDs = XP_STRDUP("");
	HANDLE hString = 0;
	*/

	GetSelection(indices, count);

	/* for (int i = 0 ; i < count; i++) {
		if ((entryID = AB_GetEntryIDAt((AddressPane*) m_pane, indices[i])) != 0) {
			AB_ExportToVCard(theApp.m_pABook, 
				(DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, m_dirIndex + 1), entryID, &pVcard);
			pVcards = StrAllocCat(pVcards, pVcard);
			XP_FREE(pVcard);
			pVcard = NULL;

			ltoa(entryID, szTemp, 10);
			sprintf(szEntryID, "%s-", szTemp);
			pEntryIDs = StrAllocCat(pEntryIDs, szEntryID);
		}
	}

	if (pVcards) {
		hString = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,strlen(pVcards)+1);
        LPSTR lpszString = (LPSTR)GlobalLock(hString);
        strcpy(lpszString,pVcards);
		XP_FREE (pVcards);
        GlobalUnlock(hString);

		HANDLE hEntryID = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT,strlen(pEntryIDs)+1);
        LPSTR lpszEntryID = (LPSTR)GlobalLock(hEntryID);
        strcpy(lpszEntryID,pEntryIDs);
		XP_FREE (pEntryIDs);
        GlobalUnlock(hEntryID);
        pDataSource->CacheGlobalData(m_cfAddresses, hString);
		pDataSource->CacheGlobalData(m_cfSourceTarget, hEntryID);
		pDataSource->CacheGlobalData(CF_TEXT, hString);
    } */

    return pDataSource;
}


BOOL CDirOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
	if ( iColumn != ID_COLDIR_TYPE )
        return CMSelectOutliner::RenderData ( iColumn, rect, dc, text );
	int idxImage;

    if (m_pDirLine->dirType == LDAPDirectory)
		idxImage = IDX_DIRLDAPAB;
	else
		idxImage = IDX_DIRPERSONALAB;

	m_pIUserImage->DrawImage ( idxImage,
		rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
	return TRUE;
}


int CDirOutliner::TranslateIcon ( void * pLineData )
{
	DIR_Server* line = (DIR_Server*) pLineData;
	int idxImage = 0;

    if (m_pDirLine->dirType == ABTypeList)
		idxImage = IDX_DIRLDAPAB;
	else
		idxImage = IDX_DIRPERSONALAB;
	return idxImage;
}

int CDirOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CDirOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

void * CDirOutliner::AcquireLineData ( int line )
{
	m_lineindex = line + 1;
	if ( line >= m_iTotalLines)
		return NULL;
	m_pDirLine = (DIR_Server*) XP_ListGetObjectNum(theApp.m_directories, line + 1);

	return m_pDirLine;
}

void CDirOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}

void CDirOutliner::ReleaseLineData ( void * )
{
}

LPCTSTR CDirOutliner::GetColumnText ( UINT iColumn, void * pLineData )
{
	DIR_Server* line = (DIR_Server*) pLineData;

	switch (iColumn) {
		case ID_COLDIR_NAME:
			return line->description;
			break;
		default:
			break;
	}
    return ("");
}

void CDirOutliner::OnSelChanged()
{
	MSG_ViewIndex *indices;
	int count;

	CAddrFrame* frame = (CAddrFrame*) GetParentFrame();
	if (frame->IsSearching()) 
		return;

	GetSelection(indices, count);

	if (count == 1) {
		frame->OnUpdateDirectorySelection(indices[0]);
	}
}

void CDirOutliner::OnSelDblClk()
{
	GetParentFrame()->SendMessage(WM_COMMAND, ID_ITEM_PROPERTIES, 0);
}


/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CDirOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CDirOutlinerParent)
	ON_WM_DESTROY()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CDirOutlinerParent::CDirOutlinerParent()
{

}


CDirOutlinerParent::~CDirOutlinerParent()
{
 
}


BOOL CDirOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CDirOutliner *pOutliner = (CDirOutliner *) m_pOutliner;
	
	// Calculate text offset from top using font height.
    TEXTMETRIC tm;
    dc.GetTextMetrics ( &tm );
    cy = ( rect.bottom - rect.top - tm.tmHeight ) / 2;
        
	// Draw Text String
	dc.TextOut (rect.left + cx, rect.top + cy, text, _tcslen(text) );
    
	// Draw Sort Indicator
	MSG_COMMAND_CHECK_STATE sortType = pOutliner->m_attribSortBy == idColumn ? MSG_Checked : MSG_Unchecked;

	int idxImage = pOutliner->m_bSortAscending ? IDX_SORTINDICATORDOWN : IDX_SORTINDICATORUP;
	CSize cs = dc.GetTextExtent(text, _tcslen(text));

	if (idColumn == pOutliner->m_attribSortBy && cs.cx + 22 <= rect.Width()) {
        m_pIImage->DrawTransImage( idxImage,
								   rect.left + 8 + cs.cx,
								   (rect.top + rect.bottom) / 2 - 4,
								   &dc );
	}

    return TRUE;
}

COutliner * CDirOutlinerParent::GetOutliner ( void )
{
    return new CDirOutliner;
}

void CDirOutlinerParent::CreateColumns ( void )
{
	int iCol0 = 0,
		iCol1 = 0;
	CString cs; 

	m_pOutliner->AddColumn ("",			ID_COLDIR_TYPE,		24, 0, ColumnFixed, 0, TRUE );
	cs.LoadString(IDS_USERNAME);
    m_pOutliner->AddColumn (cs,			ID_COLDIR_NAME,		1500, 0, ColumnVariable, iCol0 ? iCol0 : 1500 );
	m_pOutliner->SetHasPipes( FALSE );

	m_pOutliner->SetVisibleColumns(DEF_DIRVISIBLE_COLUMNS);
	m_pOutliner->LoadXPPrefs("mailnews.abook_dir_columns_win");
}

void CDirOutlinerParent::OnDestroy()
{
	int32 prefInt = ((CDirOutliner*)m_pOutliner)->GetSortBy();
	
	COutlinerParent::OnDestroy();
}

void CDirOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.abook_dir_columns_win");
}

BOOL CDirOutlinerParent::ColumnCommand ( int idColumn )
{	
	ABID lastSelection;

	// no sorting for right now
	return TRUE;

	CDirOutliner *pOutliner = (CDirOutliner *) m_pOutliner;
	
	if (pOutliner->GetFocusLine() != -1)
		lastSelection = AB_GetEntryIDAt((AddressPane*) pOutliner->GetPane(), pOutliner->GetFocusLine());

	SetCursor ( theApp.LoadStandardCursor ( IDC_WAIT ) );

	if (idColumn == pOutliner->m_attribSortBy) {
		pOutliner->m_bSortAscending = !pOutliner->m_bSortAscending; 
	}
	else
		pOutliner->m_bSortAscending = TRUE;

	pOutliner->m_attribSortBy = idColumn;

	switch (idColumn) {
		case ID_COLDIR_TYPE:
			AB_Command(pOutliner->GetPane(), AB_SortByTypeCmd, 0, 0);
			break;
		case ID_COLDIR_NAME:
			AB_Command(pOutliner->GetPane(), AB_SortByFullNameCmd, 0, 0);
			break;
		default:
			AB_Command(pOutliner->GetPane(), AB_SortByFullNameCmd, 0, 0);
			break;
	}
	

	if (pOutliner->GetFocusLine() != -1) {
		uint32 index = AB_GetIndexOfEntryID ((AddressPane *) pOutliner->GetPane(), lastSelection);
		pOutliner->SelectItem (CASTINT(index));
		pOutliner->ScrollIntoView(CASTINT(index));
	}

	Invalidate();
	pOutliner->Invalidate();
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
	return TRUE;
}


void WFE_MSGOpenAB()
{
	CAddrFrame::Open();
}


void CAddrFrame::UpdateMenu(HMENU hMenu, UINT &nID)
{
	// *** If you change the way this menu is built, you also need to
	// change CAddrFrame::FolderInfoFromMenuID

	MSG_Master *pMaster = WFE_MSGGetMaster();
	int i, j;
	MSG_FolderInfo **folderInfos;
	int32 iCount;
	MSG_FolderLine folderLine;
	char buffer[_MAX_PATH];

	j = 0;

	iCount = MSG_GetFolderChildren( pMaster, NULL, NULL, NULL );

	folderInfos = new MSG_FolderInfo*[iCount];
	if (folderInfos) {
		MSG_GetFolderChildren( pMaster, NULL, folderInfos, iCount );

		for ( i = 0; i < iCount; i++) {
			if ( MSG_GetFolderLineById( pMaster, folderInfos[ i ], &folderLine ) ) {
				if (j < 10) {
					sprintf( buffer, "&%d %s", j, folderLine.name);
				} else if ( j < 36 ) {
					sprintf( buffer, "&%c %s", 'a' + j - 10, folderLine.name);
				} else {
					sprintf( buffer, "  %s", folderLine.name);
				}
				::AppendMenu( hMenu, MF_STRING, nID, buffer);
				nID++;
				j++;
			}
		}
		delete [] folderInfos;
	}
}

void WFE_MSGBuildAddressBookPopup( HMENU hmenu, BOOL bAddSender )
{
	UINT nID;
	if (bAddSender)
		nID = FIRST_ADDSENDER_MENU_ID;
	else
		nID = FIRST_ADDALL_MENU_ID;
	HMENU hABAddMenu = CreatePopupMenu();
	HMENU hAddMenu = CreatePopupMenu();

	// ::AppendMenu( hmenu, MF_POPUP|MF_STRING, (UINT) hFileMenu, szLoadString( IDS_POPUP_FILE ) );
	::AppendMenu( hAddMenu, MF_STRING, ID_MESSAGE_ADDSENDER, szLoadString( IDS_POPUP_ADDSENDER ) );
	::AppendMenu( hAddMenu, MF_STRING, ID_MESSAGE_ADDALL, szLoadString( IDS_POPUP_ADDALL ) );
	::AppendMenu( hmenu, MF_STRING|MF_POPUP, (UINT) hAddMenu, szLoadString( IDS_POPUP_ADDTOADDRESSBOOK ) );
	::AppendMenu( hmenu, MF_SEPARATOR, 0, NULL );
	CAddrFrame::UpdateMenu( hABAddMenu, nID );
}
