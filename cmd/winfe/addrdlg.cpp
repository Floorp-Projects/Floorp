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
#include "addrdlg.h"

#ifdef MOZ_NEWADDR

#include "msg_srch.h"
#include "dirprefs.h"
#include "apiaddr.h"
#include "nethelp.h" 
#include "prefapi.h"
#include "intl_csi.h"
#include "srchdlg.h"
#include "abcom.h"


#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
#include "xpgetstr.h"
extern int MK_ADDR_CCNAME;
extern int MK_ADDR_BCCNAME;
extern int MK_ADDR_TONAME;
};

#define ADDRESS_DIALOG_TIMER				7
#define TYPEDOWN_SPEED						300
#define LDAP_SEARCH_SPEED					1250
#define ADDRDLG_OUTLINER_TYPEDOWN_TIMER     251
#define OUTLINER_TYPEDOWN_SPEED				1250


class CAddrDialog;


CAddrDialogCX::CAddrDialogCX(CAddrDialog *pDialog)  
: CStubsCX(AddressCX, MWContextAddressBook)
{
	m_pDialog = pDialog;
	m_lPercent = 0;
	m_bAnimated = FALSE;
}


void CAddrDialogCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent ) {
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

void CAddrDialogCX::Progress(MWContext *pContext, const char *pMessage) {
	m_csProgress = pMessage;
	if ( m_pDialog ) {
		m_pDialog->SetStatusText(pMessage);
	}
}

int32 CAddrDialogCX::QueryProgressPercent()	{
	return m_lPercent;
}


void CAddrDialogCX::AllConnectionsComplete(MWContext *pContext)    
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

void CAddrDialogCX::UpdateStopState( MWContext *pContext )
{
    if (m_pDialog) {
		m_pDialog->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

CWnd *CAddrDialogCX::GetDialogOwner() const {
	return m_pDialog;
}


/////////////////////////////////////////////////////////////////////////////
// CAddrDialogEntryList

class CAddrDialogEntryList: public IMsgList {

	CAddrDialog *m_pAddrDialog;
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

	CAddrDialogEntryList( CAddrDialog *pAddrDialog ) {
		m_ulRefCount = 0;
		m_pAddrDialog = pAddrDialog;
	}
};


STDMETHODIMP CAddrDialogEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) m_pAddrDialog;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CAddrDialogEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CAddrDialogEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CAddrDialogEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pAddrDialog) {
		m_pAddrDialog->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CAddrDialogEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pAddrDialog) {
		m_pAddrDialog->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CAddrDialogEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CAddrDialogEntryList::SelectItem( MSG_Pane* pane, int item )
{
}



/////////////////////////////////////////////////////////////////////////////
// CAddrDialog

CAddrDialog::CAddrDialog(
    CWnd* pParent, /*=NULL*/
    BOOL isMapi, LPSTR winText, MAPIAddressCallbackProc mapiCB, MAPIAddressGetAddrProc mapiGetProc  // rhp - MAPI
    )
	: CDialog(CAddrDialog::IDD, pParent)
{

	CString msg;
	int result = 0;
	INTL_CharSetInfo csi;

	m_pCX = new CAddrDialogCX( this );
	csi = LO_GetDocumentCharacterSetInfo(m_pCX->GetContext());

	m_pCX->GetContext()->type = MWContextAddressBook;
	m_pCX->GetContext()->fancyFTP = TRUE;
	m_pCX->GetContext()->fancyNews = TRUE;
	m_pCX->GetContext()->intrupt = FALSE;
	m_pCX->GetContext()->reSize = FALSE;
	INTL_SetCSIWinCSID(csi, CIntlWin::GetSystemLocaleCsid());

	m_pOutliner = NULL;
	m_addrBookPane = NULL;
	m_addrContPane = NULL;
	m_bSearching = FALSE;
	m_directory = 0;
	m_pDropTarget = NULL;
	m_idefButtonID = IDC_TO;

	m_EditWnd.SetOwner(this);
	CAddrDialogEntryList *pInstance = new CAddrDialogEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );


  // rhp - MAPI stuff...
  m_isMAPI = isMapi;
  m_mapiHeader = NULL;
  m_mapiCBProc = NULL;
  m_mapiGetAddrProc = NULL;
    
  if (m_isMAPI)
  {
    if ( (winText) && (*winText) )
    {
      m_mapiHeader = strdup(winText);
    }

    m_mapiCBProc = mapiCB; 
    m_mapiGetAddrProc = mapiGetProc;
  }

  	CAddrFrame::HandleErrorReturn((result = AB_CreateABPane(&m_addrBookPane,
		m_pCX->GetContext(),
		WFE_MSGGetMaster())));

	if (result)
		return;

	CAddrFrame::HandleErrorReturn((result = AB_CreateContainerPane(&m_addrContPane,
		m_pCX->GetContext(),
		WFE_MSGGetMaster())));

	if (result) {
		AB_ClosePane (m_addrBookPane);
		return;
	}

	CAddrFrame::HandleErrorReturn((result = AB_InitializeContainerPane(m_addrContPane)));

	if (result) {
		AB_ClosePane (m_addrContPane);
		AB_ClosePane (m_addrBookPane);
		return;
	}

	AB_ContainerInfo *info = AB_GetContainerForIndex(m_addrContPane, 0);

	ASSERT (info);

	CAddrFrame::HandleErrorReturn((result = AB_InitializeABPane(m_addrBookPane,
								info)));
	if (result) {
		AB_ClosePane (m_addrContPane);
		AB_ClosePane (m_addrBookPane);
		return;
	}

	//set property sheets for address book pane
	HandleErrorReturn((result = AB_SetShowPropertySheetForEntryFunc(
		(MSG_Pane *)m_addrBookPane,
		ShowPropertySheetForEntry)));

	if (result) {
		AB_ClosePane (m_addrContPane);
		AB_ClosePane (m_addrBookPane);
	}

	//set property sheets for container pane
	HandleErrorReturn((result = AB_SetShowPropertySheetForEntryFunc(
		(MSG_Pane *)m_addrContPane,
		ShowPropertySheetForEntry)));

	if (result) {
		AB_ClosePane (m_addrContPane);
		AB_ClosePane (m_addrBookPane);
	}

	//set ldap property sheets for container pane
	HandleErrorReturn((result = AB_SetShowPropertySheetForDirFunc(
		(MSG_Pane *)m_addrContPane,
		ShowPropertySheetForDir)));

	if (result) {
		AB_ClosePane (m_addrContPane);
		AB_ClosePane (m_addrBookPane);
	}

	//{{AFX_DATA_INIT(CAddrDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CAddrDialog::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	CDialog::OnKeyDown ( nChar, nRepCnt, nFlags );
}

void CAddrDialog::CleanupOnClose()
{
	// DestroyContext will call Interrupt, but if we wait until after DestroyContext
	// to call MSG_SearchFree, the MWContext will be gone, and we'll be reading freed memory

	PREF_SetIntPref("mail.addr_picker.sliderwidth", m_pSplitter->GetPaneSize());

	if (XP_IsContextBusy (m_pCX->GetContext()))
		XP_InterruptContext (m_pCX->GetContext());
	MSG_SearchFree ((MSG_Pane*) m_addrBookPane);

	if (m_pIAddrList)
		m_pIAddrList->Release();

	if (m_addrBookPane)
		HandleErrorReturn(AB_ClosePane(m_addrBookPane));

	if (m_addrContPane)
		HandleErrorReturn(AB_ClosePane(m_addrContPane));

	if(!m_pCX->IsDestroyed()) {
		m_pCX->DestroyContext();
	}
	if (m_pFont){
		theApp.ReleaseAppFont(m_pFont);
	}

	if (m_pOutlinerParent){
		delete m_pOutlinerParent;
	}

	if (m_pDirOutlinerParent){
		delete m_pDirOutlinerParent;
	}

	if (m_pSplitter){
		delete m_pSplitter;
	}

    if (m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }

  // rhp - for MAPI
  if (m_isMAPI)
  {
    if (m_mapiHeader)
      free(m_mapiHeader);
  }
}

/////////////////////////////////////////////////////////////////////////////
// CAddrDialog Overloaded methods
/////////////////////////////////////////////////////////////////////////////

// rhp - this is needed for the population of the to address type...
void
CAddrDialog::ProcessMAPIAddressPopulation(void)
{
  int     index = 0;
  LPSTR   name;
  LPSTR   address;
  int     addrType;

  if (!m_mapiGetAddrProc)
    return;

  CListBox *pBucket = GetBucket();

  while ( m_mapiGetAddrProc(&name, &address, &addrType) )
  {
    char          *formatted;
    char          tempString[512] = "";
    int           nIndex = -1;
    NSADDRESSLIST *pAddress = new NSADDRESSLIST;

    ASSERT(pAddress);
    if (!pAddress)
      continue;

    if ((name) && (*name))
    {
      strcat(tempString, name);
      strcat(tempString, " ");
    }

    if ( (!address) || (!(*address)) )
      continue;

    strcat(tempString, "<");
    strcat(tempString, address);
    strcat(tempString, ">");

    pAddress->ulHeaderType = addrType;
    pAddress->idBitmap = 0; 
    pAddress->idEntry = 0;
    pAddress->szAddress = strdup (tempString);

    GetFormattedString(tempString, addrType, &formatted);
    if (formatted) 
    {
      nIndex = pBucket->InsertString( index, formatted );
      free (formatted);
    }

    if ( nIndex < 0 )
      return;

    pBucket->SetItemDataPtr( nIndex, pAddress );
    ++index;
  }

  OnSelchange();
}

BOOL CAddrDialog::OnInitDialog( )
{
	if (CDialog::OnInitDialog()) {
		CWnd* widget;
		CRect rect2, rect3, rect4;
		UINT aIDArray[] = { IDS_SECURITY_STATUS, IDS_TRANSFER_STATUS, ID_SEPARATOR,
							IDS_ONLINE_STATUS};
		int result = 0;

		CListBox *pBucket = GetBucket();

		widget = GetDlgItem(IDC_ADDRESSLIST);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		GetClientRect(&rect4);
		ClientToScreen(&rect4);
		rect2.OffsetRect(-rect4.left, -rect4.top);

		widget->DestroyWindow ();

		// create slider control
		m_pSplitter = (CMailNewsSplitter *) RUNTIME_CLASS(CMailNewsSplitter)->CreateObject();

        ASSERT(m_pSplitter);

#ifdef _WIN32
		m_pSplitter->CreateEx(0, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_TABSTOP,
								  0, 0, 0, 0,
								  this->m_hWnd, (HMENU) IDC_ADDRESSLIST );
#else
		CCreateContext Context;
		Context.m_pCurrentFrame = NULL; //  nothing to base on
		Context.m_pCurrentDoc = NULL;  //  nothing to base on
		Context.m_pNewViewClass = NULL; // nothing to base on
		Context.m_pNewDocTemplate = NULL;   //  nothing to base on
		m_pSplitter->Create( NULL, NULL,
								 WS_CHILD|WS_VISIBLE |WS_TABSTOP,
								 rect2, this, IDC_ADDRESSLIST, &Context ); 
#endif 

		// create the outliner control
		m_pOutlinerParent = new CAddrOutlinerParent;
#ifdef _WIN32
		m_pOutlinerParent->CreateEx ( WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"), 
								WS_TABSTOP|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
								0, 0, 0, 0,
								m_pSplitter->m_hWnd, (HMENU) 99);
#else
		rect2.SetRectEmpty();
		m_pOutlinerParent->Create( NULL, _T("NSOutlinerParent"), 
								  WS_BORDER|WS_TABSTOP|WS_VISIBLE|WS_CHILD,
								  rect2, m_pSplitter, 99);
#endif

		m_pOutlinerParent->CreateColumns ( );
		m_pOutlinerParent->EnableFocusFrame(TRUE);
		m_pOutliner = (CAddrOutliner *) m_pOutlinerParent->m_pOutliner;
		m_pOutliner->SetPane(m_addrBookPane);
		m_pOutliner->SetContext( m_pCX->GetContext() );
		m_pOutliner->SetOutlinerOwner(this);


		// create the directory outliner control
		m_pDirOutlinerParent = new CDirOutlinerParent;

#ifdef _WIN32
		m_pDirOutlinerParent->CreateEx(WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"),
								  WS_TABSTOP|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
								  0,0,0,0, m_pSplitter->m_hWnd, (HMENU) 100 );
#else
		m_pDirOutlinerParent->Create( NULL, _T("NSOutlinerParent"),
								 WS_BORDER|WS_TABSTOP|WS_VISIBLE|WS_CHILD,
								 CRect(0,0,0,0), m_pSplitter, 100);
#endif

		m_pDirOutliner = (CDirOutliner *) m_pDirOutlinerParent->m_pOutliner;
		m_pDirOutlinerParent->CreateColumns ( );
		m_pDirOutlinerParent->EnableFocusFrame(TRUE);
		m_pDirOutliner->SetPane(m_addrContPane);
		m_pDirOutliner->SetOutlinerOwner(this);
		m_pDirOutliner->SelectItem(0);


	//	m_pDirOutliner->SetDirectoryIndex(0);

		// set the width of the panes in the slider
		int32 prefInt = -1;
		PREF_GetIntPref("mail.addr_picker.sliderwidth", &prefInt);
		RECT rect;
		GetClientRect(&rect);
		m_pSplitter->AddPanes(m_pDirOutlinerParent, m_pOutlinerParent, prefInt, TRUE);
		m_pSplitter->SetWindowPos(GetDlgItem(ID_NAVIGATE_INTERRUPT), rect2.left, rect2.top, rect3.right, rect3.bottom, SWP_SHOWWINDOW);

		m_pDirOutlinerParent->SetWindowPos(GetDlgItem(ID_NAVIGATE_INTERRUPT), 0, 0, 
			rect3.right, rect3.bottom, SWP_NOMOVE|SWP_NOSIZE);
		m_pOutlinerParent->SetWindowPos(m_pDirOutlinerParent, 0, 0, 
			rect3.right, rect3.bottom, SWP_NOMOVE|SWP_NOSIZE);
		// reset the tab order
		widget = GetDlgItem (IDC_TO);
		widget->SetWindowPos(m_pOutlinerParent, 0, 0, 
			rect3.right, rect3.bottom, SWP_NOMOVE|SWP_NOSIZE);
		UpdateDirectories();

		// create the status bar
		widget = GetDlgItem(IDC_StatusRect);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		widget->DestroyWindow ();

		m_barStatus.Create(this, TRUE, FALSE);

		m_barStatus.MoveWindow(&rect2, TRUE);
	
		m_barStatus.SetIndicators( aIDArray, sizeof(aIDArray) / sizeof(UINT) );

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


		DoUpdateWidget(IDC_DIRSEARCH, AB_LDAPSearchCmd, TRUE);
		DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
		UpdateMsgButtons();

		CGenericFrame *pCompose = (CGenericFrame *)GetParent();

		ApiApiPtr(api);
		LPUNKNOWN pUnk = api->CreateClassInstance(
 			APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
		if (pUnk)
		{
			LPADDRESSCONTROL pIAddressControl;
			HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
			ASSERT(hRes==NOERROR);
			LPNSADDRESSLIST pList = NULL;
			int count = pIAddressControl->GetAddressList(&pList);
			pUnk->Release();
			for (int index = 0; index < count; index++)
			{
				if (pList[index].szAddress)
				{
					if (strlen (pList[index].szAddress)) 
					{
    					NSADDRESSLIST *pAddress = new NSADDRESSLIST;
						char * formatted;
						int nIndex = -1;
						ASSERT(pAddress);
    					pAddress->ulHeaderType = pList[index].ulHeaderType;
						pAddress->idBitmap = pList[index].idBitmap; 
						pAddress->idEntry = pList[index].idEntry;
        				pAddress->szAddress = strdup (pList[index].szAddress);
						GetFormattedString(pList[index].szAddress, pList[index].ulHeaderType, &formatted);
						if (formatted) 
						{
							nIndex = pBucket->InsertString( index, formatted );
							free (formatted);
						}
						free (pList[index].szAddress);
        				if ( nIndex < 0 )
        					return 0;
        				pBucket->SetItemDataPtr( nIndex, pAddress );
					}
				}
			}
			free(pList);
			OnSelchange();
		} else 
		{
			return FALSE;
		}
	} else 
	{
		return FALSE;
	}

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

	m_EditWnd.SubclassDlgItem(IDC_ADDRNAME, this);

	::SendMessage(::GetDlgItem(m_hWnd, IDC_ADDRNAME), WM_SETFONT, (WPARAM)m_pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ADDRESSBKT), WM_SETFONT, (WPARAM)m_pFont, FALSE);

	if(!m_pDropTarget) {
	   m_pDropTarget = new CAddressPickerDropTarget(this);
	   m_pDropTarget->Register(this);
	}
	DragAcceptFiles();

	CComboBox *directory = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
	directory->SetCurSel (0);
	m_pOutliner->SetDirectoryIndex(	directory->GetCurSel());
	m_pDirOutliner->SetFocusLine(0);
	if (m_pOutliner->GetTotalLines())
		m_pOutliner->SelectItem (0);
	GetDlgItem(IDC_ADDRNAME)->SetFocus();
	SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
	GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
	GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));

  // rhp - for MAPI
  if (m_isMAPI) 
  {
    ProcessMAPIAddressPopulation();

    if (m_mapiHeader)
    {
      SetWindowText(m_mapiHeader);
    }
  }

	return FALSE;
}


void CAddrDialog::GetFormattedString(char* fullname, MSG_HEADER_SET header, char** formatted)
{

	CString formattedString;
	if (fullname) {
		switch (header) {
			case MSG_TO_HEADER_MASK:
				formattedString.Format(XP_GetString (MK_ADDR_TONAME), fullname);
				break;
			case MSG_CC_HEADER_MASK:
				formattedString.Format(XP_GetString (MK_ADDR_CCNAME), fullname);
				break;
			case MSG_BCC_HEADER_MASK:
				formattedString.Format(XP_GetString (MK_ADDR_BCCNAME), fullname);
				break;
			default:
				formattedString.Format(XP_GetString (MK_ADDR_TONAME), fullname);
		}
	}
	(*formatted) = strdup (formattedString);
}

void CAddrDialog::Progress(const char *pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}


void CAddrDialog::SetProgressBarPercent(int32 lPercent)
{
	m_barStatus.SetPercentDone (lPercent);
} // END OF	FUNCTION CAddrDialog::DrawProgressBar()


/////////////////////////////////////////////////////////////////////////////
// CAddrDialog message handlers

BEGIN_MESSAGE_MAP(CAddrDialog, CDialog)
	//{{AFX_MSG_MAP(CAddrDialog)
	ON_WM_TIMER ()
	ON_EN_SETFOCUS(IDC_ADDRNAME, OnSetFocusName)
	ON_EN_CHANGE(IDC_ADDRNAME, OnChangeName)
	ON_CBN_SELCHANGE(IDC_DIRECTORIES, OnChangeDirectory)
	ON_BN_CLICKED(IDC_DIRSEARCH, OnDirectorySearch)
	ON_BN_CLICKED(ID_NAVIGATE_INTERRUPT, OnStopSearch)
	ON_BN_CLICKED( IDC_DONE_EDITING, OnDone)
	ON_BN_CLICKED( IDCANCEL, OnCancel)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_KEYDOWN()
	ON_COMMAND(IDC_TO, OnComposeMsg)
	ON_COMMAND(IDC_CC, OnComposeCCMsg)
	ON_COMMAND(IDC_BCC, OnComposeBCCMsg)
	ON_COMMAND(ID_ITEM_PROPERTIES, OnGetProperties)
	ON_LBN_SELCHANGE(IDC_ADDRESSBKT, OnSelchange)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_LBN_SETFOCUS(IDC_ADDRESSBKT, OnSetFocusBucket)
	ON_UPDATE_COMMAND_UI(IDS_ONLINE_STATUS, OnUpdateOnlineStatus)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CAddrDialog::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == m_pOutliner->GetSafeHwnd())
	{
		if (pMsg->wParam != VK_TAB && pMsg->wParam != VK_ESCAPE && pMsg->wParam != VK_RETURN) {
			::SendMessage(pMsg->hwnd, WM_KEYDOWN, pMsg->wParam, pMsg->lParam);
			return TRUE;
		}
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == GetBucket()->GetSafeHwnd())
	{
		if (pMsg->wParam == VK_DELETE || pMsg->wParam == VK_BACK) {
			OnRemove();
			return TRUE;
		}
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == GetDlgItem(IDC_ADDRNAME)->GetSafeHwnd())
	{
		if (pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN) {
			::SendMessage(m_pOutliner->GetSafeHwnd(), WM_KEYDOWN, pMsg->wParam, pMsg->lParam);
			CWnd* widget = NULL;
			widget = GetDlgItem (GetDefaultButtonID());
			if (widget) {
				SendMessage(DM_SETDEFID, GetDefaultButtonID(), TRUE);
				widget->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
				widget = GetDlgItem (IDC_DONE_EDITING);
				widget->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
			}
			return TRUE;
		}
	}

	if (pMsg->message == WM_KEYDOWN) {	
		if (pMsg->wParam == VK_TAB) {
			HWND hwndNext = NULL;
			HWND hwndFocus = ::GetFocus();

			HWND hwndPossibleNext = ::GetNextDlgTabItem( m_hWnd, pMsg->hwnd, FALSE );

			HWND hwndPossiblePrev = ::GetNextDlgTabItem( m_hWnd, pMsg->hwnd, TRUE );

			HWND hwndDirOutliner = m_pDirOutliner ? m_pDirOutliner->m_hWnd : NULL;
			HWND hwndOutliner = m_pOutliner ? m_pOutliner->m_hWnd : NULL;
			HWND hwndSlider = m_pSplitter ? m_pSplitter->m_hWnd : NULL;
			HWND hwndPropButton = GetDlgItem(ID_ITEM_PROPERTIES) ? GetDlgItem(ID_ITEM_PROPERTIES)->m_hWnd : NULL;
			

			if ( GetKeyState(VK_SHIFT) & 0x8000 ) {

				// Tab backward
				if (hwndFocus == hwndOutliner) {
					// Handle tabbing out of outliner into the directory list
					if (m_pSplitter->IsOnePaneClosed())
						hwndNext = hwndPossiblePrev;
					else
						hwndNext = hwndDirOutliner;
				} else if ( hwndFocus == hwndDirOutliner ) {
					// Handle tabbing out of the slider
					hwndNext = hwndPossiblePrev;
				} else if ( hwndPossiblePrev == hwndSlider ) {
					// Handle tabbing back to the outliner from the 
					// to/cc/bcc buttons the bucket
					hwndNext = hwndOutliner;
				}

			} else {

				// Tab forward
				if ( hwndPossibleNext == hwndSlider ) {
					// handle tabbing into the directory list
					if (m_pSplitter->IsOnePaneClosed())
						hwndNext = hwndOutliner;
					else
						hwndNext = hwndDirOutliner;
				} else if (hwndFocus == hwndDirOutliner) {
					// Handle tabbing out of the directory list
					// and into the results list
					hwndNext = hwndOutliner;
				}

			}
			if ( hwndNext ) {
				::SetFocus( hwndNext );
				return TRUE;
			}
		}
	} // if tab && keydown 

	return CDialog::PreTranslateMessage(pMsg);
}

void CAddrDialog::UpdateDirectories()
{
	DIR_Server* dir;
	CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);

	widget->ResetContent();

	XP_List *pTraverse = theApp.m_directories;
	while(dir = (DIR_Server *)XP_ListNextObject(pTraverse)) {
		if (dir->dirType == PABDirectory || dir->dirType == LDAPDirectory)
			widget->AddString(dir->description);
	}
	if (m_directory == -1)
		widget->SetCurSel(0);
	else
		widget->SetCurSel(m_directory);

	m_pDirOutliner->UpdateCount();
}


DIR_Server * CAddrDialog::GetCurrentDirectoryServer()
{
	return (DIR_Server*) XP_ListGetObjectNum (theApp.m_directories, m_pDirOutliner->GetDirectoryIndex() + 1);
}


void CAddrDialog::OnTimer(UINT nID)
{
	CWnd::OnTimer(nID);
	if (nID == ADDRESS_DIALOG_TIMER) {
		KillTimer(m_uTypedownClock);
		if (!m_name.IsEmpty()) {
			HandleErrorReturn(AB_TypedownSearch(m_addrBookPane, m_name, MSG_VIEWINDEXNONE));

//			DIR_Server* dir;
//			dir = GetCurrentDirectoryServer();
//			if (dir->dirType == LDAPDirectory)
//				PerformListDirectory (m_name.GetBuffer(0));
//			else
//				PerformTypedown(m_name.GetBuffer(0));
			m_name.ReleaseBuffer();
		}
	}
}


void CAddrDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddrDialog)
	DDX_Text(pDX, IDC_ADDRNAME, m_name);
	DDX_CBIndex(pDX, IDC_DIRECTORIES, m_directory);
	//}}AFX_DATA_MAP
}


void CAddrDialog::OnChangeName() 
{
	UpdateData(TRUE);

	GetDlgItem(IDC_TO)->EnableWindow(FALSE);
	GetDlgItem(IDC_CC)->EnableWindow(FALSE);
	GetDlgItem(IDC_BCC)->EnableWindow(FALSE);

	// TODO: Add your control notification handler code here
	if (IsSearching()) {
		CString checkName (szLoadString(IDS_SEARCHRESULTS));
		if (stricmp (checkName, m_name) != 0) {
			OnDirectorySearch();
			SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
			GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
			GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
		}
		else
			return;
	}

	SearchText(FALSE);
}

void CAddrDialog::SearchText(BOOL bImmediately)
{

	if (m_name != m_save) {
		m_save = m_name;
		KillTimer(m_uTypedownClock);
		if (m_name.GetLength())
		{
			if(bImmediately)
			{
				//in this case we want to start a new search, not accept a result
				//into the list.
				GetDlgItem(IDC_TO)->EnableWindow(FALSE);
				GetDlgItem(IDC_CC)->EnableWindow(FALSE);
				GetDlgItem(IDC_BCC)->EnableWindow(FALSE);
				m_uTypedownClock = SetTimer(ADDRESS_DIALOG_TIMER, 0, NULL);
			}
			else
			{
				DIR_Server* dir = NULL;
				SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
				GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
				GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
				dir = GetCurrentDirectoryServer();
				if (dir->dirType == PABDirectory)
					m_uTypedownClock = SetTimer(ADDRESS_DIALOG_TIMER, TYPEDOWN_SPEED, NULL);
				else {
					int32 prefInt = LDAP_SEARCH_SPEED;
					PREF_GetIntPref("ldap_1.autoComplete.interval", &prefInt);
					m_uTypedownClock = SetTimer(ADDRESS_DIALOG_TIMER, prefInt, NULL);
				}
			}
		}
	}


}

void CAddrDialog::OnSetFocusName() 
{
	CEdit *widget = (CEdit*) GetDlgItem(IDC_ADDRNAME);
	if (widget)
		widget->SetSel(0, -1, TRUE);
	SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
	GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
	GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
}

void CAddrDialog::OnSetFocusBucket() 
{
	CListBox *pBucket = (CListBox*)GetDlgItem(IDC_ADDRESSBKT);

	if (pBucket->GetCount()) {
		SendMessage(DM_SETDEFID, IDC_DONE_EDITING, TRUE);
		GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}
}

void CAddrDialog::OnUpdateOnlineStatus(CCmdUI *pCmdUI)
{
 	pCmdUI->Enable(!NET_IsOffline());
}

void CAddrDialog::OnChangeDirectory() 
{
	// TODO: Add your control notification handler code here
	if (IsSearching())
		return;

	UpdateData(TRUE);

	if (m_directory != m_savedir) {
		m_savedir = m_directory;
		if (m_directory != -1) {
		//	if (m_pDirOutliner)
		//		m_pDirOutliner->OnChangeDirectory(m_directory);
			PerformChangeDirectory(m_directory);
		}
	}
}

void CAddrDialog::OnSelchange() 
{
	CListBox *pBucket = (CListBox*)GetDlgItem(IDC_ADDRESSBKT);

	BOOL bEnabled = (pBucket->GetCount() > 0)? TRUE : FALSE;
	pBucket->EnableWindow (bEnabled);
	GetDlgItem(IDC_DONE_EDITING)->EnableWindow(bEnabled);
	if (bEnabled) {
		SendMessage(DM_SETDEFID, IDC_DONE_EDITING, TRUE);
		GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}
	else {
		GetDlgItem(IDC_ADDRNAME)->SetFocus();
		SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
		GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}

  
	// TODO: Add your control notification handler code here
}

void CAddrDialog::DoUpdateWidget( int command, AB_CommandType cmd, BOOL bUseCheck )
{

	CWnd* widget;

    XP_Bool bSelectable = FALSE, bPlural = FALSE;
    MSG_COMMAND_CHECK_STATE sState;

	if (m_addrBookPane) {
		if (m_pOutliner) {
			MSG_ViewIndex *indices = NULL;
			int count = 0;
			m_pOutliner->GetSelection(indices, count);
			AB_CommandStatusAB2(m_addrBookPane,
							  cmd,
							  indices, count,
							  &bSelectable,
							  &sState,
							  NULL,
							  &bPlural);
		} else {
			AB_CommandStatusAB2(m_addrBookPane,
							  cmd,
							  NULL, 0,
							  &bSelectable,
							  &sState,
							  NULL,
							  &bPlural);
		}
	}

	widget = GetDlgItem ( command );
    widget->EnableWindow(bSelectable);
}


void CAddrDialog::OnDirectorySearch() 
{
	// TODO: Add your control notification handler code here
	if (m_directory != -1) 
		PerformDirectorySearch();
}

int CAddrDialog::DoModal ()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;
	return CDialog::DoModal();
}


int CAddrDialog::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	m_MailNewsResourceSwitcher.Reset();
	int res = CDialog::OnCreate(lpCreateStruct);

	MSG_SetFEData( (MSG_Pane*) m_addrBookPane, (LPVOID) (LPUNKNOWN) (LPMSGLIST) m_pIAddrList );
	MSG_SetFEData( (MSG_Pane*) m_addrContPane, (LPVOID) (LPUNKNOWN) (LPMSGLIST) m_pIAddrList );
	return res;
}

// rhp - This is for MAPI processing...
void
CAddrDialog::ProcessMAPIOnDone(void)
{
  CListBox *pBucket = GetBucket();
  
  int count = pBucket->GetCount();
  if (count != LB_ERR && count > 0)
  {
      for (int index = 0; index < count; index++)
      {
        NSADDRESSLIST * pAddress = (NSADDRESSLIST *)(pBucket->GetItemDataPtr(index));
        ASSERT(pAddress);
        if (m_mapiCBProc)
        {
          m_mapiCBProc(count, index, pAddress->ulHeaderType, 
                          pAddress->szAddress); // rhp - for MAPI
        }
        delete (pAddress);
      }
  }
  
  CDialog::OnOK();
  CleanupOnClose();
}

void CAddrDialog::OnDone() 
{
	NSADDRESSLIST* pAddressList;
	CListBox *pBucket = GetBucket();

  // rhp - for MAPI...
  if (m_isMAPI)
  {
    ProcessMAPIOnDone();
    return;
  }

	int count = pBucket->GetCount();
    if (count != LB_ERR && count > 0)
    {
        pAddressList = (NSADDRESSLIST *)calloc(count, sizeof(NSADDRESSLIST));
        if (pAddressList)
        {
            for (int index = 0; index < count; index++)
            {
				NSADDRESSLIST * pAddress = (NSADDRESSLIST *)(pBucket->GetItemDataPtr(index));
                ASSERT(pAddress);
                pAddressList[index].ulHeaderType = pAddress->ulHeaderType;
                pAddressList[index].idBitmap = pAddress->idBitmap;
                pAddressList[index].idEntry = pAddress->idEntry;
        		pAddressList[index].szAddress = pAddress->szAddress;
				delete (pAddress);
            }
        }
	
		CGenericFrame *pCompose = (CGenericFrame *)GetParent();

		ApiApiPtr(api);
		LPUNKNOWN pUnk = api->CreateClassInstance(
 			APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
		if (pUnk)
		{
			LPADDRESSCONTROL pIAddressControl;
			HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
			ASSERT(hRes==NOERROR);
			int ct = pIAddressControl->SetAddressList(pAddressList, count);
			pUnk->Release();
		}
	}

	CDialog::OnOK();
	CleanupOnClose();
}


void CAddrDialog::OnCancel() 
{
	CDialog::OnCancel();
	CleanupOnClose();
}

void CAddrDialog::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffStarting( asynchronous, notify,
												   where, num );
		}
	}
	else if(pane == (MSG_Pane*)m_addrContPane)
	{
		if(m_pDirOutliner)
		{
			m_pDirOutliner->MysticStuffStarting(asynchronous, notify, where, num);
		}
	}
}

void CAddrDialog::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffFinishing( asynchronous, notify,
												    where, num );
			UpdateMsgButtons();
		}
	}
	else if(pane == (MSG_Pane*)m_addrContPane)
	{
		if(m_pDirOutliner)
		{
			m_pDirOutliner->MysticStuffFinishing(asynchronous, notify, where, num);
			UpdateMsgButtons();
		}
	}

}


void CAddrDialog::SetStatusText(const char* pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}


void CAddrDialog::SetSearchResults(MSG_ViewIndex index, int32 num)
{

	CString csStatus;

	ASSERT(m_pOutliner);
	AB_LDAPSearchResultsAB2(m_addrBookPane, index, num);
	if (num > 1 ) {
		csStatus.Format( szLoadString(IDS_SEARCHHITS), num );
	} else if ( num > 0 ) {
		csStatus.LoadString( IDS_SEARCHONEHIT );
	} else {
		csStatus.LoadString( IDS_SEARCHNOHITS );
	}

	m_barStatus.SetWindowText( csStatus );

}

STDMETHODIMP CAddrDialog::QueryInterface(REFIID refiid, LPVOID * ppv)
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

STDMETHODIMP_(ULONG) CAddrDialog::AddRef(void)
{
	return 0; // Not a real component
}

STDMETHODIMP_(ULONG) CAddrDialog::Release(void)
{
	return 0; // Not a real component
}

// IMailFrame interface
CMailNewsFrame *CAddrDialog::GetMailNewsFrame()
{
	return (CMailNewsFrame *) NULL; 
}

MSG_Pane *CAddrDialog::GetPane()
{
	return (MSG_Pane*) m_addrBookPane;
}

void CAddrDialog::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	switch(notify)
	{
		case MSG_PaneNotifyTypeDownCompleted:
			m_pOutliner->OnTypedown(value);
			break;
		case MSG_PaneNotifyStartSearching:
			m_bSearching = TRUE;
			m_barStatus.StartAnimation();
			break;

		case MSG_PaneNotifyStopSearching:
			m_bSearching = FALSE;
			break;

	}
}


void CAddrDialog::AllConnectionsComplete( MWContext *pContext )
{
	OnStopSearch();

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


void CAddrDialog::PerformTypedown (char* name)
{
	if (!m_bSearching)
		m_pOutliner->OnTypedown (name);
}

void CAddrDialog::PerformListDirectory (char* name)
{
	// Build Search
	m_barStatus.SetWindowText( szLoadString( IDS_SEARCHING ) );
	m_bSearching = TRUE;
	GetDlgItem(IDC_DIRECTORIES)->EnableWindow(FALSE);
	GetDlgItem( IDC_DIRSEARCH)->EnableWindow(FALSE);
	GetDlgItem( ID_NAVIGATE_INTERRUPT)->EnableWindow(TRUE);
	m_pOutliner->UpdateCount();
	m_barStatus.StartAnimation();

	HandleErrorReturn(AB_SearchDirectoryAB2(m_addrBookPane, name));
}


void CAddrDialog::MoveSelections(MSG_HEADER_SET header)
{
	CString csAddress;
	MSG_ViewIndex *indices;
	int count;
	CGenericFrame * pCompose = (CGenericFrame *)GetParent();
	CListBox *pBucket = GetBucket();
	pBucket->EnableWindow (TRUE);
	ABID entryID = 0;

	m_pOutliner->GetSelection(indices, count);
	AB_AttribID attribs[2] = {AB_attribEntryType, AB_attribFullAddress};
	uint16 numAttribs;
	AB_AttributeValue *pEntryValues;

	for (int32 i = 0; i < count; i++) {
		// should we stop on error?
		char* fullname = NULL;
		CString formattedString;
		numAttribs = 2;
		AB_GetEntryAttributesForPane(m_addrBookPane, indices[i], attribs,
									 &pEntryValues,&numAttribs);

		AB_EntryType type =AB_Person;
		
		for(int j = 0; j < numAttribs; j++)
		{
			switch(pEntryValues[j].attrib)
			{
				case AB_attribEntryType: type = pEntryValues[j].u.entryType;
										 break;
				case AB_attribFullAddress: fullname = XP_STRDUP(pEntryValues[j].u.string);
										   break;
			}
		}
		if (fullname){ 
			AddStringToBucket(pBucket, header, fullname, type, entryID);
		}
		AB_FreeEntryAttributeValues(pEntryValues, numAttribs);
		
	}
	CEdit *widget = (CEdit*) GetDlgItem(IDC_ADDRNAME);
	if (widget)
		widget->SetSel(0, -1, TRUE);
	pBucket->EnableWindow (TRUE);
	SendMessage(DM_SETDEFID, IDC_DONE_EDITING, TRUE);
	GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
	GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
}

void CAddrDialog::AddStringToBucket
(CListBox *pBucket, MSG_HEADER_SET header, char* fullname, AB_EntryType type, ABID entryID)
{
	CString formattedString;
	switch (header) {
		case MSG_TO_HEADER_MASK:
			formattedString.Format(XP_GetString(MK_ADDR_TONAME), fullname);
			break;
		case MSG_CC_HEADER_MASK:
			formattedString.Format(XP_GetString(MK_ADDR_CCNAME), fullname);
			break;
		case MSG_BCC_HEADER_MASK:
			formattedString.Format(XP_GetString(MK_ADDR_BCCNAME), fullname);
			break;
		default:
			formattedString.Format(XP_GetString(MK_ADDR_TONAME), fullname);
	}

	if (pBucket->FindStringExact(0, (LPCTSTR) formattedString ) == LB_ERR) {
		int idx = pBucket->AddString(formattedString);
		if ((idx != LB_ERR) || (idx != LB_ERRSPACE))
		{
			NSADDRESSLIST *pAddress = new NSADDRESSLIST;
			ASSERT(pAddress);
    		pAddress->ulHeaderType = header;

			if (type == ABTypeList)
				pAddress->idBitmap = IDB_MAILINGLIST;          
			else
				pAddress->idBitmap = IDB_PERSON; 

			pAddress->idEntry = entryID;
			pAddress->szAddress = fullname;
        	pBucket->SetItemDataPtr( idx, pAddress );
		}
	}
	int total = pBucket->GetCount();
    if (total)
	{
		GetDlgItem(IDC_DONE_EDITING)->EnableWindow(TRUE);
		pBucket->SetCurSel(total - 1);
	}
	else
		GetDlgItem(IDC_DONE_EDITING)->EnableWindow(FALSE);
}


void CAddrDialog::OnRemove()
{
	CListBox *pBucket = GetBucket();
	int prevSel = pBucket->GetCurSel();
	if (prevSel != -1)
	{
		NSADDRESSLIST * pAddress = (NSADDRESSLIST *)(pBucket->GetItemDataPtr(prevSel));
		if (pAddress->szAddress)
			free (pAddress->szAddress);
		delete pAddress;

		int count = pBucket->DeleteString (prevSel);
		if (count) {
			if (prevSel)
				pBucket->SetCurSel (prevSel - 1);
			else
				pBucket->SetCurSel (prevSel);
		}
		OnSelchange();
	}
}


void CAddrDialog::OnComposeMsg()
{
	MoveSelections(MSG_TO_HEADER_MASK);
	m_idefButtonID = IDC_TO;
}


void CAddrDialog::OnComposeCCMsg()
{
	MoveSelections(MSG_CC_HEADER_MASK);
	m_idefButtonID = IDC_CC;
}


void CAddrDialog::OnComposeBCCMsg()
{
	MoveSelections(MSG_BCC_HEADER_MASK);
	m_idefButtonID = IDC_BCC;
}


void CAddrDialog::OnGetProperties()
{
	MSG_ViewIndex *indices;
	int count;
	HWND hwndFocus = ::GetFocus();


	HWND hwndDirOutliner = m_pDirOutliner ? m_pDirOutliner->m_hWnd : NULL;
	HWND hwndOutliner = m_pOutliner ? m_pOutliner->m_hWnd : NULL;
	if (hwndFocus == hwndDirOutliner)
	{
		m_pDirOutliner->GetSelection(indices, count);
		HandleErrorReturn(AB_CommandAB2 (m_addrContPane, AB_PropertiesCmd, indices, count));
	} 
	else
	{
		m_pOutliner->GetSelection(indices, count);
		HandleErrorReturn(AB_CommandAB2 (m_addrBookPane, AB_PropertiesCmd, indices, count));
	}
}

void CAddrDialog::HandleErrorReturn(int errorid)
{	
	if (errorid) {
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			::MessageBox(NULL, XP_GetString(errorid), s, MB_OK);
	}
}


void CAddrDialog::PerformChangeDirectory (int dirIndex)
{
	DIR_Server* dir = GetCurrentDirectoryServer();
	AB_ContainerInfo *pCurrentContainerInfo = AB_GetContainerForIndex(m_addrContPane, dirIndex);
	HandleErrorReturn(AB_ChangeABContainer(m_addrBookPane, pCurrentContainerInfo));
	m_pOutliner->SetDirectoryIndex(dirIndex);
	DoUpdateWidget(IDC_DIRSEARCH, AB_LDAPSearchCmd, TRUE);
	DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
	m_pOutliner->UpdateCount( );

	int idx = m_barStatus.CommandToIndex(IDS_SECURITY_STATUS);
	if (idx > -1) {
		UINT nID = IDS_SECURITY_STATUS;
		UINT nStyle; 
        int nWidth;
		m_barStatus.GetPaneInfo( idx, nID, nStyle, nWidth );
		if (dir->isSecure)
			m_barStatus.SetPaneInfo(idx, IDS_SECURITY_STATUS, SBPS_NORMAL, nWidth);
		else
			m_barStatus.SetPaneInfo(idx, IDS_SECURITY_STATUS, SBPS_DISABLED, nWidth);
	}
	UpdateMsgButtons();
}


void CAddrDialog::OnUpdateDirectorySelection (int dirIndex, BOOL indexChanged)
{
	if(indexChanged)
	{
		CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
		widget->SetCurSel(dirIndex);
		PerformChangeDirectory(dirIndex);
		m_save = "";
	}
}

AB_ContainerInfo * CAddrDialog::GetCurrentContainerInfo()
{
	return (AB_GetContainerForIndex(m_addrContPane, m_pDirOutliner->GetDirectoryIndex()));
}

void CAddrDialog::SelectionChanged()
{
	DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
	UpdateMsgButtons();
	CWnd* widget = NULL;
	widget = GetDlgItem (GetDefaultButtonID());
	if (widget) {
		SendMessage(DM_SETDEFID, GetDefaultButtonID(), TRUE);
		widget->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		widget = GetDlgItem (IDC_DONE_EDITING);
		widget->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}


}

void CAddrDialog::DoubleClick()
{
	SetDefaultButtonID (IDC_TO);
	MoveSelections (MSG_TO_HEADER_MASK);
}

CWnd* CAddrDialog::GetOwnerWindow()
{
	return this;
}

void CAddrDialog::OnStopSearch ()
{
	if ( m_bSearching) {
		// We've ended the search
		XP_InterruptContext( m_pCX->GetContext() );
		HandleErrorReturn(AB_FinishSearchAB2(m_addrBookPane));
		m_bSearching = FALSE;
		GetDlgItem(IDC_DIRECTORIES)->EnableWindow(TRUE);
		GetDlgItem( IDC_DIRSEARCH)->EnableWindow(TRUE);
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow (FALSE);
		m_pDirOutliner->EnableWindow(TRUE);
		m_barStatus.StopAnimation();
		if (m_pOutliner->GetTotalLines())
			m_pOutliner->SelectItem (0);
		return;
	}
}



void CAddrDialog::PerformDirectorySearch ()
{
	if ( m_bSearching) {
		OnStopSearch ();
		return;
	}


	CSearchDialog searchDlg ((UINT) IDS_ADRSEARCH, 
		(MSG_Pane*) m_addrBookPane, 
		GetCurrentDirectoryServer(),
		this);
	int result = searchDlg.DoModal(); 

	// Build Search
	if (result == IDOK)
	{
		GetDlgItem(IDC_DIRSEARCH)->EnableWindow (FALSE);
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow (TRUE);
		GetDlgItem(IDC_DIRECTORIES)->EnableWindow(FALSE);
		GetDlgItem(IDC_ADDRNAME)->SetWindowText("");

		// Build Search
		m_barStatus.SetWindowText( szLoadString( IDS_SEARCHING ) );
		m_bSearching = TRUE;
		GetDlgItem(IDC_ADDRNAME)->SetWindowText(CString (szLoadString(IDS_SEARCHRESULTS)));
		m_pOutliner->UpdateCount();
		m_pOutliner->SetFocus();
		m_pDirOutliner->EnableWindow(FALSE);

		HandleErrorReturn(AB_SearchDirectoryAB2(m_addrBookPane, NULL));
	}

}


//////////////////////////////////////////////////////////////////////////////
// CAddrDialogOutliner
BEGIN_MESSAGE_MAP(CAddrDialogOutliner, CMSelectOutliner)
	//{{AFX_MSG_MAP(CAddrDialogOutliner)
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogOutliner::CAddrDialogOutliner ( )
{
	m_attribSortBy = ID_COLADDR_NAME;
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_ADDRESSBOOK,16,16);
    }
	m_bSortAscending = TRUE;
	m_iMysticPlane = 0;
	m_dirIndex = 0;
	m_hFont = NULL;
	m_uTypedownClock = 0;
}

CAddrDialogOutliner::~CAddrDialogOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
}

DIR_Server* CAddrDialogOutliner::GetCurrentDirectoryServer ()
{
	return (DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, m_dirIndex + 1);
}

void CAddrDialogOutliner::SetDirectoryIndex(int dirIndex )
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
	ClearSelection();
	if (m_iTotalLines)
		SelectItem (0);
}


void CAddrDialogOutliner::OnTimer(UINT nID)
{
	CMSelectOutliner::OnTimer(nID);
	if (nID == ADDRDLG_OUTLINER_TYPEDOWN_TIMER) {
		KillTimer(m_uTypedownClock);
		m_psTypedown = "";
	}
}

void CAddrDialogOutliner::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
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
		m_uTypedownClock = SetTimer(ADDRDLG_OUTLINER_TYPEDOWN_TIMER, prefInt, NULL);
	}
}

void CAddrDialogOutliner::OnKillFocus( CWnd* pNewWnd )
{
	CMSelectOutliner::OnKillFocus (pNewWnd );
	m_psTypedown = "";	
	KillTimer(m_uTypedownClock);
}


void CAddrDialogOutliner::OnTypedown (char* name)
{
	MSG_ViewIndex index;
	uint	startIndex;

	if (GetFocusLine() != -1)
		startIndex = GetFocusLine();
	else
		startIndex = 0;

	AB_GetIndexMatchingTypedown(m_pane, &index, 
			(LPCTSTR) name, startIndex);
	ScrollIntoView(CASTINT(index));
	SelectItem (CASTINT(index));
}

void CAddrDialogOutliner::UpdateCount( )
{
	uint32 count;

	AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
	SetTotalLines(CASTINT(count));
}


void CAddrDialogOutliner::SetPane(ABPane *pane)
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

void CAddrDialogOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CAddrDialogOutliner::MysticStuffFinishing( XP_Bool asynchronous,
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
			&& ((CAddrDialog*)pParent)->IsSearching() && num > 0) 
		{
			((CAddrDialog*)pParent)->SetSearchResults(where, num);
			HandleInsert(where, num);
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

	if (( !--m_iMysticPlane && m_pane)) 
	{
		uint32 count;
		AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
		SetTotalLines(CASTINT(count));
		Invalidate();
		UpdateWindow();
	}
}


void CAddrDialogOutliner::SetTotalLines( int count)
{
	CMSelectOutliner::SetTotalLines(count);
	if (count = 0)
		ClearSelection();
}




BOOL CAddrDialogOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
	if ( iColumn != ID_COLADDR_TYPE )
        return CMSelectOutliner::RenderData ( iColumn, rect, dc, text );

	int idxImage = 0;

    if (m_EntryLine.entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;

	m_pIUserImage->DrawImage ( idxImage,
		rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
	return TRUE;
}


int CAddrDialogOutliner::TranslateIcon ( void * pLineData )
{
	AB_EntryLine* line = (AB_EntryLine*) pLineData;
	int idxImage = 0;

    if (line->entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;
	return idxImage;
}

int CAddrDialogOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CAddrDialogOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

HFONT CAddrDialogOutliner::GetLineFont(void *pLineData)
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


void * CAddrDialogOutliner::AcquireLineData ( int line )
{
	if ( line >= m_iTotalLines)
		return NULL;
	if (!AB_GetEntryLine(m_pane, line, &m_EntryLine ))
        return NULL;

	return &m_EntryLine;
}


void CAddrDialogOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}


void CAddrDialogOutliner::ReleaseLineData ( void * )
{
}


LPCTSTR CAddrDialogOutliner::GetColumnText ( UINT iColumn, void * pLineData )
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

void CAddrDialogOutliner::OnSelChanged()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif
	((CAddrDialog*) pParent)->DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
	((CAddrDialog*) pParent)->UpdateMsgButtons();
	CWnd* widget = NULL;
	widget = ((CAddrDialog*) pParent)->GetDlgItem (((CAddrDialog*) pParent)->GetDefaultButtonID());
	if (widget) {
		SendMessage(DM_SETDEFID, ((CAddrDialog*) pParent)->GetDefaultButtonID(), TRUE);
		widget->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		widget = ((CAddrDialog*) pParent)->GetDlgItem (IDC_DONE_EDITING);
		widget->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}
}

void CAddrDialogOutliner::OnSelDblClk()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif

	((CAddrDialog*) pParent)->SetDefaultButtonID (IDC_TO);
	((CAddrDialog*) pParent)->MoveSelections (MSG_TO_HEADER_MASK);
}

/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CAddrDialogOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CAddrDialogOutlinerParent)
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogOutlinerParent::CAddrDialogOutlinerParent()
{

}


CAddrDialogOutlinerParent::~CAddrDialogOutlinerParent()
{
}


BOOL CAddrDialogOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CAddrDialogOutliner *pOutliner = (CAddrDialogOutliner *) m_pOutliner;
	
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


COutliner * CAddrDialogOutlinerParent::GetOutliner ( void )
{
    return new CAddrDialogOutliner;
}


void CAddrDialogOutlinerParent::CreateColumns ( void )
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


BOOL CAddrDialogOutlinerParent::ColumnCommand ( int idColumn )
{	
	ABID lastSelection;

	CAddrDialogOutliner *pOutliner = (CAddrDialogOutliner *) m_pOutliner;
	
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
		uint index = CASTUINT(AB_GetIndexOfEntryID ((AddressPane*) pOutliner->GetPane(), lastSelection));
		pOutliner->SelectItem (index);
		pOutliner->ScrollIntoView(index);
	}

	Invalidate();
	pOutliner->Invalidate();
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
	return TRUE;
}

void CAddrDialogOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.abook_columns_win");
}



//////////////////////////////////////////////////////////////////////////////
// CAddrDialogDirOutliner
BEGIN_MESSAGE_MAP(CAddrDialogDirOutliner, CMSelectOutliner)
	//{{AFX_MSG_MAP(CAddrDialogDirOutliner)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogDirOutliner::CAddrDialogDirOutliner ( )
{
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_DIRLIST,16,16);
    }
	m_iMysticPlane = 0;
	m_dirIndex = 0;
	m_hFont = NULL;
}

CAddrDialogDirOutliner::~CAddrDialogDirOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
}


void CAddrDialogDirOutliner::SetDirectoryIndex(int dirIndex )
{
	m_dirIndex = dirIndex;
	SelectItem (CASTINT(m_dirIndex));
}


void CAddrDialogDirOutliner::UpdateCount( )
{
	m_iTotalLines = XP_ListCount (theApp.m_directories);
	if (!m_iTotalLines)
		ClearSelection();
	Invalidate();
	UpdateWindow();
}


void CAddrDialogDirOutliner::SetPane(ABPane *pane)
{
	m_pane = pane;

	if (m_pane) {
		SetTotalLines(XP_ListCount (theApp.m_directories));
		Invalidate();
		UpdateWindow();
	}
}

void CAddrDialogDirOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CAddrDialogDirOutliner::MysticStuffFinishing( XP_Bool asynchronous,
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


void CAddrDialogDirOutliner::SetTotalLines( int count)
{
	CMSelectOutliner::SetTotalLines(count);
	if (count = 0)
		ClearSelection();
}




BOOL CAddrDialogDirOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
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


int CAddrDialogDirOutliner::TranslateIcon ( void * pLineData )
{
	DIR_Server* line = (DIR_Server*) pLineData;
	int idxImage = 0;

    if (m_pDirLine->dirType == ABTypeList)
		idxImage = IDX_DIRLDAPAB;
	else
		idxImage = IDX_DIRPERSONALAB;
	return idxImage;
}

int CAddrDialogDirOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CAddrDialogDirOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

HFONT CAddrDialogDirOutliner::GetLineFont(void *pLineData)
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


void * CAddrDialogDirOutliner::AcquireLineData ( int line )
{
	if ( line >= m_iTotalLines)
		return NULL;
	m_pDirLine = (DIR_Server*) XP_ListGetObjectNum(theApp.m_directories, line + 1);

	return m_pDirLine;
}


void CAddrDialogDirOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}


void CAddrDialogDirOutliner::ReleaseLineData ( void * )
{
}


LPCTSTR CAddrDialogDirOutliner::GetColumnText ( UINT iColumn, void * pLineData )
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

void CAddrDialogDirOutliner::OnSelChanged()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif
	MSG_ViewIndex *indices;
	int count;

	if (((CAddrDialog*) pParent)->IsSearching())
		return;

	GetSelection(indices, count);

	if (count == 1) 
		((CAddrDialog*) pParent)->OnUpdateDirectorySelection(indices[0], TRUE);
}

void CAddrDialogDirOutliner::OnSelDblClk()
{

}

/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CAddrDialogDirOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CAddrDialogDirOutlinerParent)
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogDirOutlinerParent::CAddrDialogDirOutlinerParent()
{

}


CAddrDialogDirOutlinerParent::~CAddrDialogDirOutlinerParent()
{
}


BOOL CAddrDialogDirOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CAddrDialogDirOutliner *pOutliner = (CAddrDialogDirOutliner *) m_pOutliner;
	
	// Calculate text offset from top using font height.
    TEXTMETRIC tm;
    dc.GetTextMetrics ( &tm );
    cy = ( rect.bottom - rect.top - tm.tmHeight ) / 2;
        
	// Draw Text String
	dc.TextOut (rect.left + cx, rect.top + cy, text, _tcslen(text) );

    return TRUE;
}


COutliner * CAddrDialogDirOutlinerParent::GetOutliner ( void )
{
    return new CAddrDialogDirOutliner;
}


void CAddrDialogDirOutlinerParent::CreateColumns ( void )
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


void CAddrDialogDirOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.abook_dir_columns_win");
}



void CAddrDialog::OnHelp() 
{
	NetHelp(HELP_SELECT_ADDRESSES);
}


BOOL CAddrDialog::IsDragInListBox(CPoint *pPoint)
{
	CRect listRect;

	CListBox *pBucket = GetBucket();
	pBucket->GetWindowRect(LPRECT(listRect));
	ScreenToClient(LPRECT(listRect));
	if (listRect.PtInRect(*pPoint))
		return TRUE;
	else
		return FALSE;
}

BOOL CAddrDialog::ProcessVCardData(COleDataObject * pDataObject, CPoint &point)
{
	UINT clipFormat;
	BOOL retVal = TRUE;;
	CWnd * pWnd = GetFocus();
	XP_List * pEntries;
	int32 iEntries;

	if (pDataObject->IsDataAvailable(
		clipFormat = ::RegisterClipboardFormat(vCardClipboardFormat))) 
	{
		HGLOBAL hAddresses = pDataObject->GetGlobalData(clipFormat);
		LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
		ASSERT(pAddresses);
		if (!AB_ConvertVCardsToExpandedName(theApp.m_pABook,pAddresses,&pEntries,&iEntries))
		{
			CListBox *pBucket = GetBucket();

			XP_List * node = pEntries;
			for (int32 i = 0; i < iEntries+1; i++)
			{
				char * pString = (char *)node->object;
				if (pString != NULL)
					AddStringToBucket(pBucket, MSG_TO_HEADER_MASK, pString, AB_Person, -1);
				node = node->next;
				if (!node)
					break;
            }
            XP_ListDestroy(pEntries);
		}
	}

	if (pWnd && ::IsWindow(pWnd->m_hWnd))
		pWnd->SetFocus();
	return retVal;
}

void CAddrDialog::UpdateMsgButtons()
{
	if (m_pOutliner)
	{
		MSG_ViewIndex *indices;
		int count;
		BOOL bEnable;

		m_pOutliner->GetSelection(indices, count);
		bEnable = (count > 0) ? TRUE : FALSE;
		GetDlgItem(IDC_TO)->EnableWindow(bEnable);
		GetDlgItem(IDC_CC)->EnableWindow(bEnable);
		GetDlgItem(IDC_BCC)->EnableWindow(bEnable);
#ifdef MOZ_NEWADDR
		GetDlgItem(ID_ITEM_PROPERTIES)->EnableWindow(bEnable);
#else
		//leave disabled for now.
		GetDlgItem(ID_ITEM_PROPERTIES)->EnableWindow(FALSE);
#endif
	}
	DoUpdateWidget(IDC_DIRSEARCH, AB_LDAPSearchCmd, TRUE);
	if (m_bSearching)
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow(TRUE);
	else
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow(FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// CAddressPickerDropTarget

DROPEFFECT CAddressPickerDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	// Only interested in vcard
	if(pDataObject->IsDataAvailable(
      ::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		if (m_pOwner->IsDragInListBox(&point))
			deReturn = DROPEFFECT_COPY;
	}
	return(deReturn);
} 

BOOL CAddressPickerDropTarget::OnDrop
(CWnd * pWnd, COleDataObject * pDataObject, DROPEFFECT, CPoint point)
{
	if (pDataObject->IsDataAvailable(::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		return m_pOwner->ProcessVCardData(pDataObject,point);
	}
	return FALSE;
}




#else //MOZ_NEWADDR

#include "msg_srch.h"
#include "dirprefs.h"
#include "apiaddr.h"
#include "nethelp.h" 
#include "prefapi.h"
#include "intl_csi.h"
#include "srchdlg.h"


#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
#include "xpgetstr.h"
extern int MK_ADDR_CCNAME;
extern int MK_ADDR_BCCNAME;
extern int MK_ADDR_TONAME;
};

#define ADDRESS_DIALOG_TIMER				7
#define TYPEDOWN_SPEED						300
#define LDAP_SEARCH_SPEED					1250
#define ADDRDLG_OUTLINER_TYPEDOWN_TIMER     251
#define OUTLINER_TYPEDOWN_SPEED				1250


class CAddrDialog;


CAddrDialogCX::CAddrDialogCX(CAddrDialog *pDialog)  
: CStubsCX(AddressCX, MWContextAddressBook)
{
	m_pDialog = pDialog;
	m_lPercent = 0;
	m_bAnimated = FALSE;
}


void CAddrDialogCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent ) {
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

void CAddrDialogCX::Progress(MWContext *pContext, const char *pMessage) {
	m_csProgress = pMessage;
	if ( m_pDialog ) {
		m_pDialog->SetStatusText(pMessage);
	}
}

int32 CAddrDialogCX::QueryProgressPercent()	{
	return m_lPercent;
}


void CAddrDialogCX::AllConnectionsComplete(MWContext *pContext)    
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

void CAddrDialogCX::UpdateStopState( MWContext *pContext )
{
    if (m_pDialog) {
		m_pDialog->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
}

CWnd *CAddrDialogCX::GetDialogOwner() const {
	return m_pDialog;
}


/////////////////////////////////////////////////////////////////////////////
// CAddrDialogEntryList

class CAddrDialogEntryList: public IMsgList {

	CAddrDialog *m_pAddrDialog;
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

	CAddrDialogEntryList( CAddrDialog *pAddrDialog ) {
		m_ulRefCount = 0;
		m_pAddrDialog = pAddrDialog;
	}
};


STDMETHODIMP CAddrDialogEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) m_pAddrDialog;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CAddrDialogEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CAddrDialogEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CAddrDialogEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pAddrDialog) {
		m_pAddrDialog->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CAddrDialogEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pAddrDialog) {
		m_pAddrDialog->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CAddrDialogEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CAddrDialogEntryList::SelectItem( MSG_Pane* pane, int item )
{
}



/////////////////////////////////////////////////////////////////////////////
// CAddrDialog

CAddrDialog::CAddrDialog(
    CWnd* pParent, /*=NULL*/
    BOOL isMapi, LPSTR winText, MAPIAddressCallbackProc mapiCB, MAPIAddressGetAddrProc mapiGetProc  // rhp - MAPI
    )
	: CDialog(CAddrDialog::IDD, pParent)
{

	CString msg;
	int result = 0;
	INTL_CharSetInfo csi;

	m_pCX = new CAddrDialogCX( this );
	csi = LO_GetDocumentCharacterSetInfo(m_pCX->GetContext());

	m_pCX->GetContext()->type = MWContextAddressBook;
	m_pCX->GetContext()->fancyFTP = TRUE;
	m_pCX->GetContext()->fancyNews = TRUE;
	m_pCX->GetContext()->intrupt = FALSE;
	m_pCX->GetContext()->reSize = FALSE;
	INTL_SetCSIWinCSID(csi, CIntlWin::GetSystemLocaleCsid());

	m_pOutliner = NULL;
	m_addrBookPane = NULL;
	m_bSearching = FALSE;
	m_directory = 0;
	m_pDropTarget = NULL;
	m_idefButtonID = IDC_TO;

	CAddrDialogEntryList *pInstance = new CAddrDialogEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );

	HandleErrorReturn((result = AB_CreateAddressBookPane(&m_addrBookPane,
			m_pCX->GetContext(),
			WFE_MSGGetMaster())));

  // rhp - MAPI stuff...
  m_isMAPI = isMapi;
  m_mapiHeader = NULL;
  m_mapiCBProc = NULL;
  m_mapiGetAddrProc = NULL;
    
  if (m_isMAPI)
  {
    if ( (winText) && (*winText) )
    {
      m_mapiHeader = strdup(winText);
    }

    m_mapiCBProc = mapiCB; 
    m_mapiGetAddrProc = mapiGetProc;
  }

	//{{AFX_DATA_INIT(CAddrDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CAddrDialog::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	CDialog::OnKeyDown ( nChar, nRepCnt, nFlags );
}

void CAddrDialog::CleanupOnClose()
{
	// DestroyContext will call Interrupt, but if we wait until after DestroyContext
	// to call MSG_SearchFree, the MWContext will be gone, and we'll be reading freed memory

	PREF_SetIntPref("mail.addr_picker.sliderwidth", m_pSplitter->GetPaneSize());

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

	if (m_pDirOutlinerParent){
		delete m_pDirOutlinerParent;
	}

	if (m_pSplitter){
		delete m_pSplitter;
	}

    if (m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }

  // rhp - for MAPI
  if (m_isMAPI)
  {
    if (m_mapiHeader)
      free(m_mapiHeader);
  }
}

/////////////////////////////////////////////////////////////////////////////
// CAddrDialog Overloaded methods
/////////////////////////////////////////////////////////////////////////////

// rhp - this is needed for the population of the to address type...
void
CAddrDialog::ProcessMAPIAddressPopulation(void)
{
  int     index = 0;
  LPSTR   name;
  LPSTR   address;
  int     addrType;

  if (!m_mapiGetAddrProc)
    return;

  CListBox *pBucket = GetBucket();

  while ( m_mapiGetAddrProc(&name, &address, &addrType) )
  {
    char          *formatted;
    char          tempString[512] = "";
    int           nIndex = -1;
    NSADDRESSLIST *pAddress = new NSADDRESSLIST;

    ASSERT(pAddress);
    if (!pAddress)
      continue;

    if ((name) && (*name))
    {
      strcat(tempString, name);
      strcat(tempString, " ");
    }

    if ( (!address) || (!(*address)) )
      continue;

    strcat(tempString, "<");
    strcat(tempString, address);
    strcat(tempString, ">");

    pAddress->ulHeaderType = addrType;
    pAddress->idBitmap = 0; 
    pAddress->idEntry = 0;
    pAddress->szAddress = strdup (tempString);

    GetFormattedString(tempString, addrType, &formatted);
    if (formatted) 
    {
      nIndex = pBucket->InsertString( index, formatted );
      free (formatted);
    }

    if ( nIndex < 0 )
      return;

    pBucket->SetItemDataPtr( nIndex, pAddress );
    ++index;
  }

  OnSelchange();
}

BOOL CAddrDialog::OnInitDialog( )
{
	if (CDialog::OnInitDialog()) {
		CWnd* widget;
		CRect rect2, rect3, rect4;
		UINT aIDArray[] = { IDS_SECURITY_STATUS, IDS_TRANSFER_STATUS, ID_SEPARATOR};
		int result = 0;

		CListBox *pBucket = GetBucket();

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

		widget = GetDlgItem(IDC_ADDRESSLIST);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		GetClientRect(&rect4);
		ClientToScreen(&rect4);
		rect2.OffsetRect(-rect4.left, -rect4.top);

		widget->DestroyWindow ();

		// create slider control
		m_pSplitter = (CMailNewsSplitter *) RUNTIME_CLASS(CMailNewsSplitter)->CreateObject();

        ASSERT(m_pSplitter);

#ifdef _WIN32
		m_pSplitter->CreateEx(0, NULL, NULL,
								  WS_CHILD|WS_VISIBLE|WS_TABSTOP,
								  0, 0, 0, 0,
								  this->m_hWnd, (HMENU) IDC_ADDRESSLIST );
#else
		CCreateContext Context;
		Context.m_pCurrentFrame = NULL; //  nothing to base on
		Context.m_pCurrentDoc = NULL;  //  nothing to base on
		Context.m_pNewViewClass = NULL; // nothing to base on
		Context.m_pNewDocTemplate = NULL;   //  nothing to base on
		m_pSplitter->Create( NULL, NULL,
								 WS_CHILD|WS_VISIBLE |WS_TABSTOP,
								 rect2, this, IDC_ADDRESSLIST, &Context ); 
#endif 

		// create the outliner control
		m_pOutlinerParent = new CAddrDialogOutlinerParent;
#ifdef _WIN32
		m_pOutlinerParent->CreateEx ( WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"), 
								WS_TABSTOP|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
								0, 0, 0, 0,
								m_pSplitter->m_hWnd, (HMENU) 99);
#else
		rect2.SetRectEmpty();
		m_pOutlinerParent->Create( NULL, _T("NSOutlinerParent"), 
								  WS_BORDER|WS_TABSTOP|WS_VISIBLE|WS_CHILD,
								  rect2, m_pSplitter, 99);
#endif

		m_pOutlinerParent->CreateColumns ( );
		m_pOutlinerParent->EnableFocusFrame(TRUE);
		m_pOutliner = (CAddrDialogOutliner *) m_pOutlinerParent->m_pOutliner;
		m_pOutliner->SetPane(m_addrBookPane);
		m_pOutliner->SetContext( m_pCX->GetContext() );

		// create the directory outliner control
		m_pDirOutlinerParent = new CAddrDialogDirOutlinerParent;

#ifdef _WIN32
		m_pDirOutlinerParent->CreateEx(WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"),
								  WS_TABSTOP|WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
								  0,0,0,0, m_pSplitter->m_hWnd, (HMENU) 100 );
#else
		m_pDirOutlinerParent->Create( NULL, _T("NSOutlinerParent"),
								 WS_BORDER|WS_TABSTOP|WS_VISIBLE|WS_CHILD,
								 CRect(0,0,0,0), m_pSplitter, 100);
#endif

		m_pDirOutliner = (CAddrDialogDirOutliner *) m_pDirOutlinerParent->m_pOutliner;
		m_pDirOutlinerParent->CreateColumns ( );
		m_pDirOutlinerParent->EnableFocusFrame(TRUE);
		m_pDirOutliner->SetContext( m_pCX->GetContext() );
		m_pDirOutliner->SetPane(m_addrBookPane);
		m_pDirOutliner->SetDirectoryIndex(0);

		// set the width of the panes in the slider
		int32 prefInt = -1;
		PREF_GetIntPref("mail.addr_picker.sliderwidth", &prefInt);
		RECT rect;
		GetClientRect(&rect);
		m_pSplitter->AddPanes(m_pDirOutlinerParent, m_pOutlinerParent, prefInt, TRUE);
		m_pSplitter->SetWindowPos(GetDlgItem(ID_NAVIGATE_INTERRUPT), rect2.left, rect2.top, rect3.right, rect3.bottom, SWP_SHOWWINDOW);

		m_pDirOutlinerParent->SetWindowPos(GetDlgItem(ID_NAVIGATE_INTERRUPT), 0, 0, 
			rect3.right, rect3.bottom, SWP_NOMOVE|SWP_NOSIZE);
		m_pOutlinerParent->SetWindowPos(m_pDirOutlinerParent, 0, 0, 
			rect3.right, rect3.bottom, SWP_NOMOVE|SWP_NOSIZE);
		// reset the tab order
		widget = GetDlgItem (IDC_TO);
		widget->SetWindowPos(m_pOutlinerParent, 0, 0, 
			rect3.right, rect3.bottom, SWP_NOMOVE|SWP_NOSIZE);
		UpdateDirectories();

		// create the status bar
		widget = GetDlgItem(IDC_StatusRect);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		widget->DestroyWindow ();

		m_barStatus.Create(this, TRUE, FALSE);

		m_barStatus.MoveWindow(&rect2, TRUE);
	
		m_barStatus.SetIndicators( aIDArray, sizeof(aIDArray) / sizeof(UINT) );

		DoUpdateWidget(IDC_DIRSEARCH, AB_LDAPSearchCmd, TRUE);
		DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
		UpdateMsgButtons();

		CGenericFrame *pCompose = (CGenericFrame *)GetParent();

		ApiApiPtr(api);
		LPUNKNOWN pUnk = api->CreateClassInstance(
 			APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
		if (pUnk)
		{
			LPADDRESSCONTROL pIAddressControl;
			HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
			ASSERT(hRes==NOERROR);
			LPNSADDRESSLIST pList = NULL;
			int count = pIAddressControl->GetAddressList(&pList);
			pUnk->Release();
			for (int index = 0; index < count; index++)
			{
				if (pList[index].szAddress)
				{
					if (strlen (pList[index].szAddress)) 
					{
    					NSADDRESSLIST *pAddress = new NSADDRESSLIST;
						char * formatted;
						int nIndex = -1;
						ASSERT(pAddress);
    					pAddress->ulHeaderType = pList[index].ulHeaderType;
						pAddress->idBitmap = pList[index].idBitmap; 
						pAddress->idEntry = pList[index].idEntry;
        				pAddress->szAddress = strdup (pList[index].szAddress);
						GetFormattedString(pList[index].szAddress, pList[index].ulHeaderType, &formatted);
						if (formatted) 
						{
							nIndex = pBucket->InsertString( index, formatted );
							free (formatted);
						}
						free (pList[index].szAddress);
        				if ( nIndex < 0 )
        					return 0;
        				pBucket->SetItemDataPtr( nIndex, pAddress );
					}
				}
			}
			free(pList);
			OnSelchange();
		} else 
		{
			return FALSE;
		}
	} else 
	{
		return FALSE;
	}

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

	::SendMessage(::GetDlgItem(m_hWnd, IDC_ADDRNAME), WM_SETFONT, (WPARAM)m_pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ADDRESSBKT), WM_SETFONT, (WPARAM)m_pFont, FALSE);

	if(!m_pDropTarget) {
	   m_pDropTarget = new CAddressPickerDropTarget(this);
	   m_pDropTarget->Register(this);
	}
	DragAcceptFiles();

	CComboBox *directory = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
	directory->SetCurSel (0);
	m_pOutliner->SetDirectoryIndex(	directory->GetCurSel());
	m_pDirOutliner->SetFocusLine(0);
	if (m_pOutliner->GetTotalLines())
		m_pOutliner->SelectItem (0);
	GetDlgItem(IDC_ADDRNAME)->SetFocus();
	SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
	GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
	GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));

  // rhp - for MAPI
  if (m_isMAPI) 
  {
    ProcessMAPIAddressPopulation();

    if (m_mapiHeader)
    {
      SetWindowText(m_mapiHeader);
    }
  }

	return FALSE;
}


void CAddrDialog::GetFormattedString(char* fullname, MSG_HEADER_SET header, char** formatted)
{

	CString formattedString;
	if (fullname) {
		switch (header) {
			case MSG_TO_HEADER_MASK:
				formattedString.Format(XP_GetString (MK_ADDR_TONAME), fullname);
				break;
			case MSG_CC_HEADER_MASK:
				formattedString.Format(XP_GetString (MK_ADDR_CCNAME), fullname);
				break;
			case MSG_BCC_HEADER_MASK:
				formattedString.Format(XP_GetString (MK_ADDR_BCCNAME), fullname);
				break;
			default:
				formattedString.Format(XP_GetString (MK_ADDR_TONAME), fullname);
		}
	}
	(*formatted) = strdup (formattedString);
}

void CAddrDialog::Progress(const char *pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}


void CAddrDialog::SetProgressBarPercent(int32 lPercent)
{
	m_barStatus.SetPercentDone (lPercent);
} // END OF	FUNCTION CAddrDialog::DrawProgressBar()


/////////////////////////////////////////////////////////////////////////////
// CAddrDialog message handlers

BEGIN_MESSAGE_MAP(CAddrDialog, CDialog)
	//{{AFX_MSG_MAP(CAddrDialog)
	ON_WM_TIMER ()
	ON_EN_SETFOCUS(IDC_ADDRNAME, OnSetFocusName)
	ON_EN_CHANGE(IDC_ADDRNAME, OnChangeName)
	ON_CBN_SELCHANGE(IDC_DIRECTORIES, OnChangeDirectory)
	ON_BN_CLICKED(IDC_DIRSEARCH, OnDirectorySearch)
	ON_BN_CLICKED(ID_NAVIGATE_INTERRUPT, OnStopSearch)
	ON_BN_CLICKED( IDC_DONE_EDITING, OnDone)
	ON_BN_CLICKED( IDCANCEL, OnCancel)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_KEYDOWN()
	ON_COMMAND(IDC_TO, OnComposeMsg)
	ON_COMMAND(IDC_CC, OnComposeCCMsg)
	ON_COMMAND(IDC_BCC, OnComposeBCCMsg)
	ON_COMMAND(ID_ITEM_PROPERTIES, OnGetProperties)
	ON_LBN_SELCHANGE(IDC_ADDRESSBKT, OnSelchange)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_LBN_SETFOCUS(IDC_ADDRESSBKT, OnSetFocusBucket)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CAddrDialog::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == m_pOutliner->GetSafeHwnd())
	{
		if (pMsg->wParam != VK_TAB && pMsg->wParam != VK_ESCAPE && pMsg->wParam != VK_RETURN) {
			::SendMessage(pMsg->hwnd, WM_KEYDOWN, pMsg->wParam, pMsg->lParam);
			return TRUE;
		}
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == GetBucket()->GetSafeHwnd())
	{
		if (pMsg->wParam == VK_DELETE || pMsg->wParam == VK_BACK) {
			OnRemove();
			return TRUE;
		}
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == GetDlgItem(IDC_ADDRNAME)->GetSafeHwnd())
	{
		if (pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN) {
			::SendMessage(m_pOutliner->GetSafeHwnd(), WM_KEYDOWN, pMsg->wParam, pMsg->lParam);
			CWnd* widget = NULL;
			widget = GetDlgItem (GetDefaultButtonID());
			if (widget) {
				SendMessage(DM_SETDEFID, GetDefaultButtonID(), TRUE);
				widget->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
				widget = GetDlgItem (IDC_DONE_EDITING);
				widget->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
			}
			return TRUE;
		}
	}

	if (pMsg->message == WM_KEYDOWN) {	
		if (pMsg->wParam == VK_TAB) {
			HWND hwndNext = NULL;
			HWND hwndFocus = ::GetFocus();

			HWND hwndPossibleNext = ::GetNextDlgTabItem( m_hWnd, pMsg->hwnd, FALSE );

			HWND hwndPossiblePrev = ::GetNextDlgTabItem( m_hWnd, pMsg->hwnd, TRUE );

			HWND hwndDirOutliner = m_pDirOutliner ? m_pDirOutliner->m_hWnd : NULL;
			HWND hwndOutliner = m_pOutliner ? m_pOutliner->m_hWnd : NULL;
			HWND hwndSlider = m_pSplitter ? m_pSplitter->m_hWnd : NULL;
			HWND hwndDefSendButton = GetDlgItem(m_idefButtonID) ? GetDlgItem(m_idefButtonID)->m_hWnd : NULL;
			HWND hwndPropButton = GetDlgItem(ID_ITEM_PROPERTIES) ? GetDlgItem(ID_ITEM_PROPERTIES)->m_hWnd : NULL;
				

			if ( GetKeyState(VK_SHIFT) & 0x8000 ) {

				// Tab backward
				if ( hwndFocus == hwndPropButton ) {
					// Handle tabbing back to the to/cc/bcc buttons
					if (::IsWindowEnabled (hwndDefSendButton)) {
						hwndNext = hwndDefSendButton;
						DWORD defID = SendMessage(DM_GETDEFID, 0, 0);
						int defaultID = LOWORD (defID);
						SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
						GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
						if (defaultID != m_idefButtonID)
							GetDlgItem(defaultID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
					}
				} else if (hwndFocus == hwndOutliner) {
					// Handle tabbing out of outliner into the directory list
					if (m_pSplitter->IsOnePaneClosed())
						hwndNext = hwndPossiblePrev;
					else
						hwndNext = hwndDirOutliner;
				} else if ( hwndFocus == hwndDirOutliner ) {
					// Handle tabbing out of the slider
					hwndNext = hwndPossiblePrev;
				} else if ( hwndPossiblePrev == hwndSlider ) {
					// Handle tabbing back to the outliner from the 
					// to/cc/bcc buttons the bucket
					hwndNext = hwndOutliner;
				}

			} else {

				// Tab forward
				if (hwndFocus == hwndOutliner) {
					// Handle tabbing out of outliner
					if (::IsWindowEnabled (hwndDefSendButton)) {
						hwndNext = hwndDefSendButton;
						DWORD defID = SendMessage(DM_GETDEFID, 0, 0);
						int defaultID = LOWORD (defID);
						SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
						GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
						if (defaultID != m_idefButtonID)
							GetDlgItem(defaultID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
					}
				} else if ( hwndPossibleNext == hwndSlider ) {
					// handle tabbing into the directory list
					if (m_pSplitter->IsOnePaneClosed())
						hwndNext = hwndOutliner;
					else
						hwndNext = hwndDirOutliner;
				} else if (hwndFocus == hwndDirOutliner) {
					// Handle tabbing out of the directory list
					// and into the results list
					hwndNext = hwndOutliner;
				}

			}
			if ( hwndNext ) {
				::SetFocus( hwndNext );
				return TRUE;
			}
		}
	} // if tab && keydown 

	return CDialog::PreTranslateMessage(pMsg);
}

void CAddrDialog::UpdateDirectories()
{
	DIR_Server* dir;
	CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);

	widget->ResetContent();

	XP_List *pTraverse = theApp.m_directories;
	while(dir = (DIR_Server *)XP_ListNextObject(pTraverse)) {
		if (dir->dirType == PABDirectory || dir->dirType == LDAPDirectory)
			widget->AddString(dir->description);
	}
	if (m_directory == -1)
		widget->SetCurSel(0);
	else
		widget->SetCurSel(m_directory);

	m_pDirOutliner->UpdateCount();
}


DIR_Server * CAddrDialog::GetCurrentDirectoryServer()
{
	return (DIR_Server*) XP_ListGetObjectNum (theApp.m_directories, m_pOutliner->GetDirectoryIndex() + 1);
}


void CAddrDialog::OnTimer(UINT nID)
{
	CWnd::OnTimer(nID);
	if (nID == ADDRESS_DIALOG_TIMER) {
		KillTimer(m_uTypedownClock);
		if (!m_name.IsEmpty()) {
			DIR_Server* dir;
			dir = GetCurrentDirectoryServer();
			if (dir->dirType == LDAPDirectory)
				PerformListDirectory (m_name.GetBuffer(0));
			else
				PerformTypedown(m_name.GetBuffer(0));
			m_name.ReleaseBuffer();
		}
	}
}


void CAddrDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddrDialog)
	DDX_Text(pDX, IDC_ADDRNAME, m_name);
	DDX_CBIndex(pDX, IDC_DIRECTORIES, m_directory);
	//}}AFX_DATA_MAP
}


void CAddrDialog::OnChangeName() 
{
	UpdateData(TRUE);

	// TODO: Add your control notification handler code here
	if (IsSearching()) {
		CString checkName (szLoadString(IDS_SEARCHRESULTS));
		if (stricmp (checkName, m_name) != 0) {
			OnDirectorySearch();
			SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
			GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
			GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
		}
		else
			return;
	}

	if (m_name != m_save) {
		m_save = m_name;
		KillTimer(m_uTypedownClock);
		if (m_name.GetLength())
		{
			DIR_Server* dir = NULL;
			SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
			GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
			GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
			dir = GetCurrentDirectoryServer();
			if (dir->dirType == PABDirectory)
				m_uTypedownClock = SetTimer(ADDRESS_DIALOG_TIMER, TYPEDOWN_SPEED, NULL);
			else {
				int32 prefInt = LDAP_SEARCH_SPEED;
				PREF_GetIntPref("ldap_1.autoComplete.interval", &prefInt);
				m_uTypedownClock = SetTimer(ADDRESS_DIALOG_TIMER, prefInt, NULL);
			}
		}
	}
}


void CAddrDialog::OnSetFocusName() 
{
	CEdit *widget = (CEdit*) GetDlgItem(IDC_ADDRNAME);
	if (widget)
		widget->SetSel(0, -1, TRUE);
	SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
	GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
	GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
}

void CAddrDialog::OnSetFocusBucket() 
{
	CListBox *pBucket = (CListBox*)GetDlgItem(IDC_ADDRESSBKT);

	if (pBucket->GetCount()) {
		SendMessage(DM_SETDEFID, IDC_DONE_EDITING, TRUE);
		GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}
}

void CAddrDialog::OnChangeDirectory() 
{
	// TODO: Add your control notification handler code here
	if (IsSearching())
		return;

	UpdateData(TRUE);

	if (m_directory != m_savedir) {
		m_savedir = m_directory;
		if (m_directory != -1) {
			if (m_pDirOutliner)
				m_pDirOutliner->SetDirectoryIndex(m_directory);
			PerformChangeDirectory(m_directory);
		}
	}
}

void CAddrDialog::OnSelchange() 
{
	CListBox *pBucket = (CListBox*)GetDlgItem(IDC_ADDRESSBKT);

	BOOL bEnabled = (pBucket->GetCount() > 0)? TRUE : FALSE;
	pBucket->EnableWindow (bEnabled);
	GetDlgItem(IDC_DONE_EDITING)->EnableWindow(bEnabled);
	if (bEnabled) {
		SendMessage(DM_SETDEFID, IDC_DONE_EDITING, TRUE);
		GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}
	else {
		GetDlgItem(IDC_ADDRNAME)->SetFocus();
		SendMessage(DM_SETDEFID, m_idefButtonID, TRUE);
		GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}

  
	// TODO: Add your control notification handler code here
}

void CAddrDialog::DoUpdateWidget( int command, AB_CommandType cmd, BOOL bUseCheck )
{

	CWnd* widget;

    XP_Bool bSelectable = FALSE, bPlural = FALSE;
    MSG_COMMAND_CHECK_STATE sState;

	if (m_addrBookPane) {
		if (m_pOutliner) {
			MSG_ViewIndex *indices = NULL;
			int count = 0;
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

	widget = GetDlgItem ( command );
    widget->EnableWindow(bSelectable);
}


void CAddrDialog::OnDirectorySearch() 
{
	// TODO: Add your control notification handler code here
	if (m_directory != -1) 
		PerformDirectorySearch();
}

int CAddrDialog::DoModal ()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;
	return CDialog::DoModal();
}


int CAddrDialog::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	m_MailNewsResourceSwitcher.Reset();
	int res = CDialog::OnCreate(lpCreateStruct);

	MSG_SetFEData( (MSG_Pane*) m_addrBookPane, (void *) m_pIAddrList );
	return res;
}

// rhp - This is for MAPI processing...
void
CAddrDialog::ProcessMAPIOnDone(void)
{
  CListBox *pBucket = GetBucket();
  
  int count = pBucket->GetCount();
  if (count != LB_ERR && count > 0)
  {
      for (int index = 0; index < count; index++)
      {
        NSADDRESSLIST * pAddress = (NSADDRESSLIST *)(pBucket->GetItemDataPtr(index));
        ASSERT(pAddress);
        if (m_mapiCBProc)
        {
          m_mapiCBProc(count, index, pAddress->ulHeaderType, 
                          pAddress->szAddress); // rhp - for MAPI
        }
        delete (pAddress);
      }
  }
  
  CDialog::OnOK();
  CleanupOnClose();
}

void CAddrDialog::OnDone() 
{
	NSADDRESSLIST* pAddressList;
	CListBox *pBucket = GetBucket();

  // rhp - for MAPI...
  if (m_isMAPI)
  {
    ProcessMAPIOnDone();
    return;
  }

	int count = pBucket->GetCount();
    if (count != LB_ERR && count > 0)
    {
        pAddressList = (NSADDRESSLIST *)calloc(count, sizeof(NSADDRESSLIST));
        if (pAddressList)
        {
            for (int index = 0; index < count; index++)
            {
				NSADDRESSLIST * pAddress = (NSADDRESSLIST *)(pBucket->GetItemDataPtr(index));
                ASSERT(pAddress);
                pAddressList[index].ulHeaderType = pAddress->ulHeaderType;
                pAddressList[index].idBitmap = pAddress->idBitmap;
                pAddressList[index].idEntry = pAddress->idEntry;
        		pAddressList[index].szAddress = pAddress->szAddress;
				delete (pAddress);
            }
        }
	
		CGenericFrame *pCompose = (CGenericFrame *)GetParent();

		ApiApiPtr(api);
		LPUNKNOWN pUnk = api->CreateClassInstance(
 			APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
		if (pUnk)
		{
			LPADDRESSCONTROL pIAddressControl;
			HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
			ASSERT(hRes==NOERROR);
			int ct = pIAddressControl->SetAddressList(pAddressList, count);
			pUnk->Release();
		}
	}

	CDialog::OnOK();
	CleanupOnClose();
}


void CAddrDialog::OnCancel() 
{
	CDialog::OnCancel();
	CleanupOnClose();
}

void CAddrDialog::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
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

void CAddrDialog::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		if ( m_pOutliner ) {
			m_pOutliner->MysticStuffFinishing( asynchronous, notify,
												    where, num );
			UpdateMsgButtons();
		}
	}
}


void CAddrDialog::SetStatusText(const char* pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}


void CAddrDialog::SetSearchResults(MSG_ViewIndex index, int32 num)
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

STDMETHODIMP CAddrDialog::QueryInterface(REFIID refiid, LPVOID * ppv)
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

STDMETHODIMP_(ULONG) CAddrDialog::AddRef(void)
{
	return 0; // Not a real component
}

STDMETHODIMP_(ULONG) CAddrDialog::Release(void)
{
	return 0; // Not a real component
}

// IMailFrame interface
CMailNewsFrame *CAddrDialog::GetMailNewsFrame()
{
	return (CMailNewsFrame *) NULL; 
}

MSG_Pane *CAddrDialog::GetPane()
{
	return (MSG_Pane*) m_addrBookPane;
}

void CAddrDialog::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if (notify == MSG_PaneDirectoriesChanged) {
		if (IsSearching())
			OnStopSearch ();
		CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
		m_savedir = -1;
		widget->SetCurSel (0);
		OnChangeDirectory();
		UpdateDirectories();
	}
}


void CAddrDialog::AllConnectionsComplete( MWContext *pContext )
{
	OnStopSearch();

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


void CAddrDialog::PerformTypedown (char* name)
{
	if (!m_bSearching)
		m_pOutliner->OnTypedown (name);
}

void CAddrDialog::PerformListDirectory (char* name)
{
	// Build Search
	m_barStatus.SetWindowText( szLoadString( IDS_SEARCHING ) );
	m_bSearching = TRUE;
	GetDlgItem(IDC_DIRECTORIES)->EnableWindow(FALSE);
	GetDlgItem( IDC_DIRSEARCH)->EnableWindow(FALSE);
	GetDlgItem( ID_NAVIGATE_INTERRUPT)->EnableWindow(TRUE);
	m_pOutliner->UpdateCount();
	m_barStatus.StartAnimation();

	HandleErrorReturn(AB_SearchDirectory(m_addrBookPane, name));
}


void CAddrDialog::MoveSelections(MSG_HEADER_SET header)
{
	CString csAddress;
	MSG_ViewIndex *indices;
	int count;
	CGenericFrame * pCompose = (CGenericFrame *)GetParent();
	CListBox *pBucket = GetBucket();
	pBucket->EnableWindow (TRUE);

	m_pOutliner->GetSelection(indices, count);

	// delete the entries 
	for (int32 i = 0; i < count; i++) {
		// should we stop on error?
		char* fullname = NULL;
		CString formattedString;
		ABID entryID = AB_GetEntryIDAt((AddressPane*) m_addrBookPane, indices[i]);
		AB_GetExpandedName(GetCurrentDirectoryServer(), 
			theApp.m_pABook, entryID, &fullname);

		if (fullname){ 
			ABID type;

			// need to change the bitmap based on the type
			AB_GetType (GetCurrentDirectoryServer(), 
				theApp.m_pABook, entryID, &type);
			AddStringToBucket(pBucket, header, fullname, type, entryID);
		}
	}
	CEdit *widget = (CEdit*) GetDlgItem(IDC_ADDRNAME);
	if (widget)
		widget->SetSel(0, -1, TRUE);
	pBucket->EnableWindow (TRUE);
	SendMessage(DM_SETDEFID, IDC_DONE_EDITING, TRUE);
	GetDlgItem(IDC_DONE_EDITING)->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
	GetDlgItem(m_idefButtonID)->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
}

void CAddrDialog::AddStringToBucket
(CListBox *pBucket, MSG_HEADER_SET header, char* fullname, ABID type, ABID entryID)
{
	CString formattedString;
	switch (header) {
		case MSG_TO_HEADER_MASK:
			formattedString.Format(XP_GetString(MK_ADDR_TONAME), fullname);
			break;
		case MSG_CC_HEADER_MASK:
			formattedString.Format(XP_GetString(MK_ADDR_CCNAME), fullname);
			break;
		case MSG_BCC_HEADER_MASK:
			formattedString.Format(XP_GetString(MK_ADDR_BCCNAME), fullname);
			break;
		default:
			formattedString.Format(XP_GetString(MK_ADDR_TONAME), fullname);
	}

	if (pBucket->FindStringExact(0, (LPCTSTR) formattedString ) == LB_ERR) {
		int idx = pBucket->AddString(formattedString);
		if ((idx != LB_ERR) || (idx != LB_ERRSPACE))
		{
			NSADDRESSLIST *pAddress = new NSADDRESSLIST;
			ASSERT(pAddress);
    		pAddress->ulHeaderType = header;

			if (type == ABTypeList)
				pAddress->idBitmap = IDB_MAILINGLIST;          
			else
				pAddress->idBitmap = IDB_PERSON; 

			pAddress->idEntry = entryID;
			pAddress->szAddress = fullname;
        	pBucket->SetItemDataPtr( idx, pAddress );
		}
	}
	int total = pBucket->GetCount();
    if (total)
	{
		GetDlgItem(IDC_DONE_EDITING)->EnableWindow(TRUE);
		pBucket->SetCurSel(total - 1);
	}
	else
		GetDlgItem(IDC_DONE_EDITING)->EnableWindow(FALSE);
}


void CAddrDialog::OnRemove()
{
	CListBox *pBucket = GetBucket();
	int prevSel = pBucket->GetCurSel();
	if (prevSel != -1)
	{
		NSADDRESSLIST * pAddress = (NSADDRESSLIST *)(pBucket->GetItemDataPtr(prevSel));
		if (pAddress->szAddress)
			free (pAddress->szAddress);
		delete pAddress;

		int count = pBucket->DeleteString (prevSel);
		if (count) {
			if (prevSel)
				pBucket->SetCurSel (prevSel - 1);
			else
				pBucket->SetCurSel (prevSel);
		}
		OnSelchange();
	}
}


void CAddrDialog::OnComposeMsg()
{
	MoveSelections(MSG_TO_HEADER_MASK);
	m_idefButtonID = IDC_TO;
}


void CAddrDialog::OnComposeCCMsg()
{
	MoveSelections(MSG_CC_HEADER_MASK);
	m_idefButtonID = IDC_CC;
}


void CAddrDialog::OnComposeBCCMsg()
{
	MoveSelections(MSG_BCC_HEADER_MASK);
	m_idefButtonID = IDC_BCC;
}


void CAddrDialog::OnGetProperties()
{
	MSG_ViewIndex *indices = NULL;
	int count;
	CAddrDialogOutliner *pOutliner = (CAddrDialogOutliner *) m_pOutliner;

	if (pOutliner)
	{
		pOutliner->GetSelection(indices, count);
	//	AB_Command(pOutliner->GetPane(), AB_ImportLdapEntriesCmd, indices, count);
	}
}

void CAddrDialog::HandleErrorReturn(int errorid)
{	
	if (errorid) {
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			::MessageBox(NULL, XP_GetString(errorid), s, MB_OK);
	}
}


void CAddrDialog::PerformChangeDirectory (int dirIndex)
{
	m_pOutliner->SetDirectoryIndex(dirIndex);
	DIR_Server* dir = GetCurrentDirectoryServer();
	HandleErrorReturn(AB_ChangeDirectory(m_addrBookPane, dir));
	DoUpdateWidget(IDC_DIRSEARCH, AB_LDAPSearchCmd, TRUE);
	DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
	m_pOutliner->UpdateCount( );

	int idx = m_barStatus.CommandToIndex(IDS_SECURITY_STATUS);
	if (idx > -1) {
		UINT nID = IDS_SECURITY_STATUS;
		UINT nStyle; 
        int nWidth;
		m_barStatus.GetPaneInfo( idx, nID, nStyle, nWidth );
		if (dir->isSecure)
			m_barStatus.SetPaneInfo(idx, IDS_SECURITY_STATUS, SBPS_NORMAL, nWidth);
		else
			m_barStatus.SetPaneInfo(idx, IDS_SECURITY_STATUS, SBPS_DISABLED, nWidth);
	}
	UpdateMsgButtons();
}


void CAddrDialog::OnUpdateDirectorySelection (int dirIndex)
{
	CComboBox *widget = (CComboBox*) GetDlgItem(IDC_DIRECTORIES);
	widget->SetCurSel(dirIndex);
	PerformChangeDirectory(dirIndex);
}


void CAddrDialog::OnStopSearch ()
{
	if ( m_bSearching) {
		// We've ended the search
		XP_InterruptContext( m_pCX->GetContext() );
		HandleErrorReturn(AB_FinishSearch(m_addrBookPane, m_pCX->GetContext()));
		m_bSearching = FALSE;
		GetDlgItem(IDC_DIRECTORIES)->EnableWindow(TRUE);
		GetDlgItem( IDC_DIRSEARCH)->EnableWindow(TRUE);
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow (FALSE);
		m_pDirOutliner->EnableWindow(TRUE);
		m_barStatus.StopAnimation();
		if (m_pOutliner->GetTotalLines())
			m_pOutliner->SelectItem (0);
		return;
	}
}



void CAddrDialog::PerformDirectorySearch ()
{
	if ( m_bSearching) {
		OnStopSearch ();
		return;
	}


	CSearchDialog searchDlg ((UINT) IDS_ADRSEARCH, 
		(MSG_Pane*) m_addrBookPane, 
		GetCurrentDirectoryServer(),
		this);
	int result = searchDlg.DoModal(); 

	// Build Search
	if (result == IDOK)
	{
		GetDlgItem(IDC_DIRSEARCH)->EnableWindow (FALSE);
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow (TRUE);
		GetDlgItem(IDC_DIRECTORIES)->EnableWindow(FALSE);
		GetDlgItem(IDC_ADDRNAME)->SetWindowText("");

		// Build Search
		m_barStatus.SetWindowText( szLoadString( IDS_SEARCHING ) );
		m_bSearching = TRUE;
		GetDlgItem(IDC_ADDRNAME)->SetWindowText(CString (szLoadString(IDS_SEARCHRESULTS)));
		m_pOutliner->UpdateCount();
		m_pOutliner->SetFocus();
		m_pDirOutliner->EnableWindow(FALSE);

		HandleErrorReturn(AB_SearchDirectory(m_addrBookPane, NULL));
	}

}


//////////////////////////////////////////////////////////////////////////////
// CAddrDialogOutliner
BEGIN_MESSAGE_MAP(CAddrDialogOutliner, CMSelectOutliner)
	//{{AFX_MSG_MAP(CAddrDialogOutliner)
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogOutliner::CAddrDialogOutliner ( )
{
	m_attribSortBy = ID_COLADDR_NAME;
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_ADDRESSBOOK,16,16);
    }
	m_bSortAscending = TRUE;
	m_iMysticPlane = 0;
	m_dirIndex = 0;
	m_hFont = NULL;
	m_uTypedownClock = 0;
}

CAddrDialogOutliner::~CAddrDialogOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
}

DIR_Server* CAddrDialogOutliner::GetCurrentDirectoryServer ()
{
	return (DIR_Server*)XP_ListGetObjectNum(theApp.m_directories, m_dirIndex + 1);
}

void CAddrDialogOutliner::SetDirectoryIndex(int dirIndex )
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
	ClearSelection();
	if (m_iTotalLines)
		SelectItem (0);
}


void CAddrDialogOutliner::OnTimer(UINT nID)
{
	CMSelectOutliner::OnTimer(nID);
	if (nID == ADDRDLG_OUTLINER_TYPEDOWN_TIMER) {
		KillTimer(m_uTypedownClock);
		m_psTypedown = "";
	}
}

void CAddrDialogOutliner::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags )
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
		m_uTypedownClock = SetTimer(ADDRDLG_OUTLINER_TYPEDOWN_TIMER, prefInt, NULL);
	}
}

void CAddrDialogOutliner::OnKillFocus( CWnd* pNewWnd )
{
	CMSelectOutliner::OnKillFocus (pNewWnd );
	m_psTypedown = "";	
	KillTimer(m_uTypedownClock);
}


void CAddrDialogOutliner::OnTypedown (char* name)
{
	MSG_ViewIndex index;
	uint	startIndex;

	if (GetFocusLine() != -1)
		startIndex = GetFocusLine();
	else
		startIndex = 0;

	AB_GetIndexMatchingTypedown(m_pane, &index, 
			(LPCTSTR) name, startIndex);
	ScrollIntoView(CASTINT(index));
	SelectItem (CASTINT(index));
}

void CAddrDialogOutliner::UpdateCount( )
{
	uint32 count;

	AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
	SetTotalLines(CASTINT(count));
}


void CAddrDialogOutliner::SetPane(ABPane *pane)
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

void CAddrDialogOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CAddrDialogOutliner::MysticStuffFinishing( XP_Bool asynchronous,
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
			&& ((CAddrDialog*)pParent)->IsSearching() && num > 0) 
		{
			((CAddrDialog*)pParent)->SetSearchResults(where, num);
			HandleInsert(where, num);
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

	if (( !--m_iMysticPlane && m_pane)) 
	{
		uint32 count;
		AB_GetEntryCount(GetCurrentDirectoryServer (), theApp.m_pABook, &count, ABTypeAll, NULL);
		SetTotalLines(CASTINT(count));
		Invalidate();
		UpdateWindow();
	}
}


void CAddrDialogOutliner::SetTotalLines( int count)
{
	CMSelectOutliner::SetTotalLines(count);
	if (count = 0)
		ClearSelection();
}




BOOL CAddrDialogOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
	if ( iColumn != ID_COLADDR_TYPE )
        return CMSelectOutliner::RenderData ( iColumn, rect, dc, text );

	int idxImage = 0;

    if (m_EntryLine.entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;

	m_pIUserImage->DrawImage ( idxImage,
		rect.left + ( ( rect.Width ( ) - 16 ) / 2 ), rect.top, &dc, FALSE );
	return TRUE;
}


int CAddrDialogOutliner::TranslateIcon ( void * pLineData )
{
	AB_EntryLine* line = (AB_EntryLine*) pLineData;
	int idxImage = 0;

    if (line->entryType == ABTypeList)
		idxImage = IDX_ADDRESSBOOKLIST;
	else
		idxImage = IDX_ADDRESSBOOKPERSON;
	return idxImage;
}

int CAddrDialogOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CAddrDialogOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

HFONT CAddrDialogOutliner::GetLineFont(void *pLineData)
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


void * CAddrDialogOutliner::AcquireLineData ( int line )
{
	m_lineindex = line + 1;
	if ( line >= m_iTotalLines)
		return NULL;
	if (!AB_GetEntryLine(m_pane, line, &m_EntryLine ))
        return NULL;

	return &m_EntryLine;
}


void CAddrDialogOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}


void CAddrDialogOutliner::ReleaseLineData ( void * )
{
}


LPCTSTR CAddrDialogOutliner::GetColumnText ( UINT iColumn, void * pLineData )
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

void CAddrDialogOutliner::OnSelChanged()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif
	((CAddrDialog*) pParent)->DoUpdateWidget(IDC_TO, AB_NewMessageCmd, TRUE);
	((CAddrDialog*) pParent)->UpdateMsgButtons();
	CWnd* widget = NULL;
	widget = ((CAddrDialog*) pParent)->GetDlgItem (((CAddrDialog*) pParent)->GetDefaultButtonID());
	if (widget) {
		SendMessage(DM_SETDEFID, ((CAddrDialog*) pParent)->GetDefaultButtonID(), TRUE);
		widget->SendMessage(BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
		widget = ((CAddrDialog*) pParent)->GetDlgItem (IDC_DONE_EDITING);
		widget->SendMessage(BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
	}
}

void CAddrDialogOutliner::OnSelDblClk()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif

	((CAddrDialog*) pParent)->SetDefaultButtonID (IDC_TO);
	((CAddrDialog*) pParent)->MoveSelections (MSG_TO_HEADER_MASK);
}

/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CAddrDialogOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CAddrDialogOutlinerParent)
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogOutlinerParent::CAddrDialogOutlinerParent()
{

}


CAddrDialogOutlinerParent::~CAddrDialogOutlinerParent()
{
}


BOOL CAddrDialogOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CAddrDialogOutliner *pOutliner = (CAddrDialogOutliner *) m_pOutliner;
	
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


COutliner * CAddrDialogOutlinerParent::GetOutliner ( void )
{
    return new CAddrDialogOutliner;
}


void CAddrDialogOutlinerParent::CreateColumns ( void )
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


BOOL CAddrDialogOutlinerParent::ColumnCommand ( int idColumn )
{	
	ABID lastSelection;

	CAddrDialogOutliner *pOutliner = (CAddrDialogOutliner *) m_pOutliner;
	
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
		uint index = CASTUINT(AB_GetIndexOfEntryID ((AddressPane*) pOutliner->GetPane(), lastSelection));
		pOutliner->SelectItem (index);
		pOutliner->ScrollIntoView(index);
	}

	Invalidate();
	pOutliner->Invalidate();
	SetCursor ( theApp.LoadStandardCursor ( IDC_ARROW ) );
	return TRUE;
}

void CAddrDialogOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.abook_columns_win");
}



//////////////////////////////////////////////////////////////////////////////
// CAddrDialogDirOutliner
BEGIN_MESSAGE_MAP(CAddrDialogDirOutliner, CMSelectOutliner)
	//{{AFX_MSG_MAP(CAddrDialogDirOutliner)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogDirOutliner::CAddrDialogDirOutliner ( )
{
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_DIRLIST,16,16);
    }
	m_iMysticPlane = 0;
	m_dirIndex = 0;
	m_hFont = NULL;
}

CAddrDialogDirOutliner::~CAddrDialogDirOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	if (m_hFont) {
		theApp.ReleaseAppFont(m_hFont);
	}
}


void CAddrDialogDirOutliner::SetDirectoryIndex(int dirIndex )
{
	m_dirIndex = dirIndex;
	SelectItem (CASTINT(m_dirIndex));
}


void CAddrDialogDirOutliner::UpdateCount( )
{
	m_iTotalLines = XP_ListCount (theApp.m_directories);
	if (!m_iTotalLines)
		ClearSelection();
	Invalidate();
	UpdateWindow();
}


void CAddrDialogDirOutliner::SetPane(ABPane *pane)
{
	m_pane = pane;

	if (m_pane) {
		SetTotalLines(XP_ListCount (theApp.m_directories));
		Invalidate();
		UpdateWindow();
	}
}

void CAddrDialogDirOutliner::MysticStuffStarting( XP_Bool asynchronous,
											MSG_NOTIFY_CODE notify, 
											MSG_ViewIndex where,
											int32 num )
{
	++m_iMysticPlane;
}

void CAddrDialogDirOutliner::MysticStuffFinishing( XP_Bool asynchronous,
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


void CAddrDialogDirOutliner::SetTotalLines( int count)
{
	CMSelectOutliner::SetTotalLines(count);
	if (count = 0)
		ClearSelection();
}




BOOL CAddrDialogDirOutliner::RenderData  ( UINT iColumn, CRect &rect, CDC &dc, const char * text )
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


int CAddrDialogDirOutliner::TranslateIcon ( void * pLineData )
{
	DIR_Server* line = (DIR_Server*) pLineData;
	int idxImage = 0;

    if (m_pDirLine->dirType == ABTypeList)
		idxImage = IDX_DIRLDAPAB;
	else
		idxImage = IDX_DIRPERSONALAB;
	return idxImage;
}

int CAddrDialogDirOutliner::TranslateIconFolder (void * pData)
{
	// We're a flat list, so we're never a folder
    return ( OUTLINER_ITEM );
}


BOOL CAddrDialogDirOutliner::ColumnCommand ( int iColumn, int iLine )
{
	// We have no column commands
    return FALSE;
}

HFONT CAddrDialogDirOutliner::GetLineFont(void *pLineData)
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


void * CAddrDialogDirOutliner::AcquireLineData ( int line )
{
	m_lineindex = line + 1;
	if ( line >= m_iTotalLines)
		return NULL;
	m_pDirLine = (DIR_Server*) XP_ListGetObjectNum(theApp.m_directories, line + 1);

	return m_pDirLine;
}


void CAddrDialogDirOutliner::GetTreeInfo ( int iLine, uint32 * pFlags, int * pDepth, OutlinerAncestorInfo ** pAncestor )
{
    if ( pFlags ) *pFlags = 0; // Flags? 
    if ( pDepth ) *pDepth = 0; // We're flat, remember?
}


void CAddrDialogDirOutliner::ReleaseLineData ( void * )
{
}


LPCTSTR CAddrDialogDirOutliner::GetColumnText ( UINT iColumn, void * pLineData )
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

void CAddrDialogDirOutliner::OnSelChanged()
{
#ifdef _WIN32
	CWnd *pParent = GetParentOwner();
#else
	CWnd *pParent = GetOwner();      
	pParent = pParent->GetParent();
	ASSERT(pParent);
#endif
	MSG_ViewIndex *indices;
	int count;

	if (((CAddrDialog*) pParent)->IsSearching())
		return;

	GetSelection(indices, count);

	if (count == 1) 
		((CAddrDialog*) pParent)->OnUpdateDirectorySelection(indices[0]);
}

void CAddrDialogDirOutliner::OnSelDblClk()
{

}

/////////////////////////////////////////////////////////////////////////////
// CAddrOutlinerParent

BEGIN_MESSAGE_MAP(CAddrDialogDirOutlinerParent, COutlinerParent)
	//{{AFX_MSG_MAP(CAddrDialogDirOutlinerParent)
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAddrDialogDirOutlinerParent::CAddrDialogDirOutlinerParent()
{

}


CAddrDialogDirOutlinerParent::~CAddrDialogDirOutlinerParent()
{
}


BOOL CAddrDialogDirOutlinerParent::RenderData ( int idColumn, CRect & rect, CDC &dc, const char * text )
{
	int cx = 3, cy = 0;
    CAddrDialogDirOutliner *pOutliner = (CAddrDialogDirOutliner *) m_pOutliner;
	
	// Calculate text offset from top using font height.
    TEXTMETRIC tm;
    dc.GetTextMetrics ( &tm );
    cy = ( rect.bottom - rect.top - tm.tmHeight ) / 2;
        
	// Draw Text String
	dc.TextOut (rect.left + cx, rect.top + cy, text, _tcslen(text) );

    return TRUE;
}


COutliner * CAddrDialogDirOutlinerParent::GetOutliner ( void )
{
    return new CAddrDialogDirOutliner;
}


void CAddrDialogDirOutlinerParent::CreateColumns ( void )
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


void CAddrDialogDirOutlinerParent::OnLButtonUp( UINT nFlags, CPoint point )
{
	BOOL bSaveColumns = (m_pOutliner && 
		(m_bResizeColumn || m_bDraggingHeader || m_iPusherHit))? TRUE : FALSE;
	COutlinerParent::OnLButtonUp(nFlags, point);
	if (bSaveColumns)
		m_pOutliner->SaveXPPrefs("mailnews.abook_dir_columns_win");
}



void CAddrDialog::OnHelp() 
{
	NetHelp(HELP_SELECT_ADDRESSES);
}


BOOL CAddrDialog::IsDragInListBox(CPoint *pPoint)
{
	CRect listRect;

	CListBox *pBucket = GetBucket();
	pBucket->GetWindowRect(LPRECT(listRect));
	ScreenToClient(LPRECT(listRect));
	if (listRect.PtInRect(*pPoint))
		return TRUE;
	else
		return FALSE;
}

BOOL CAddrDialog::ProcessVCardData(COleDataObject * pDataObject, CPoint &point)
{
	UINT clipFormat;
	BOOL retVal = TRUE;;
	CWnd * pWnd = GetFocus();
	XP_List * pEntries;
	int32 iEntries;

	if (pDataObject->IsDataAvailable(
		clipFormat = ::RegisterClipboardFormat(vCardClipboardFormat))) 
	{
		HGLOBAL hAddresses = pDataObject->GetGlobalData(clipFormat);
		LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
		ASSERT(pAddresses);
		if (!AB_ConvertVCardsToExpandedName(theApp.m_pABook,pAddresses,&pEntries,&iEntries))
		{
			CListBox *pBucket = GetBucket();

			XP_List * node = pEntries;
			for (int32 i = 0; i < iEntries+1; i++)
			{
				char * pString = (char *)node->object;
				if (pString != NULL)
					AddStringToBucket(pBucket, MSG_TO_HEADER_MASK, pString, ABTypePerson, -1);
				node = node->next;
				if (!node)
					break;
            }
            XP_ListDestroy(pEntries);
		}
	}

	if (pWnd && ::IsWindow(pWnd->m_hWnd))
		pWnd->SetFocus();
	return retVal;
}

void CAddrDialog::UpdateMsgButtons()
{
	if (m_pOutliner)
	{
		MSG_ViewIndex *indices;
		int count;
		BOOL bEnable;

		m_pOutliner->GetSelection(indices, count);
		bEnable = (count > 0) ? TRUE : FALSE;
		GetDlgItem(IDC_TO)->EnableWindow(bEnable);
		GetDlgItem(IDC_CC)->EnableWindow(bEnable);
		GetDlgItem(IDC_BCC)->EnableWindow(bEnable);
#ifdef MOZ_NEWADDR
		GetDlgItem(ID_ITEM_PROPERTIES)->EnableWindow(bEnable);
#else
		//leave disabled for now.
		GetDlgItem(ID_ITEM_PROPERTIES)->EnableWindow(FALSE);
#endif
	}
	DoUpdateWidget(IDC_DIRSEARCH, AB_LDAPSearchCmd, TRUE);
	if (m_bSearching)
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow(TRUE);
	else
		GetDlgItem(ID_NAVIGATE_INTERRUPT)->EnableWindow(FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// CAddressPickerDropTarget

DROPEFFECT CAddressPickerDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	// Only interested in vcard
	if(pDataObject->IsDataAvailable(
      ::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		if (m_pOwner->IsDragInListBox(&point))
			deReturn = DROPEFFECT_COPY;
	}
	return(deReturn);
} 

BOOL CAddressPickerDropTarget::OnDrop
(CWnd * pWnd, COleDataObject * pDataObject, DROPEFFECT, CPoint point)
{
	if (pDataObject->IsDataAvailable(::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		return m_pOwner->ProcessVCardData(pDataObject,point);
	}
	return FALSE;
}


#endif // MOZ_NEWADDR
