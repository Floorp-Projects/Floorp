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
// abmldlg.cpp : implementation file
//

#include "stdafx.h"
#include "abmldlg.h"

#ifdef MOZ_NEWADDR

#include "addrfrm.h"
#include "template.h"
#include "wfemsg.h"
#include "msg_srch.h"
#include "dirprefs.h"
#include "compfrm.h"
#include "apiaddr.h"
#include "nethelp.h"	// help
#include "xp_help.h"
#include "intl_csi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
#include "xpgetstr.h"
extern int MK_ADDR_ADD_PERSON_TO_ABOOK;
extern int MK_UNABLE_TO_OPEN_ADDR_FILE;
};

#define NUM_MAILING_LIST_ATTRIBUTES 3
#define NUM_MAILING_LIST_ENTRY_ATTRIBUTES 2



/////////////////////////////////////////////////////////////////////////////
// CAddrDialogEntryList

class CABMLDialogEntryList: public IMsgList {

	CABMLDialog *m_pABMLDialog;
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

	CABMLDialogEntryList( CABMLDialog *pAddrDialog ) {
		m_ulRefCount = 0;
		m_pABMLDialog = pAddrDialog;
	}
};


STDMETHODIMP CABMLDialogEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CABMLDialogEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CABMLDialogEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CABMLDialogEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pABMLDialog) {
		m_pABMLDialog->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CABMLDialogEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pABMLDialog) {
		m_pABMLDialog->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CABMLDialogEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CABMLDialogEntryList::SelectItem( MSG_Pane* pane, int item )
{
}



/////////////////////////////////////////////////////////////////////////////
// CAddrDialog

CABMLDialog::CABMLDialog(CWnd* pParent, MSG_Pane *pane, MWContext* context)
	: CDialog(CABMLDialog::IDD, pParent)
{
	CString msg;

	//{{AFX_DATA_INIT(CAddrList)
	m_description = _T("");
	m_name = _T("");
	m_nickname = _T("");
	//}}AFX_DATA_INIT

	m_addingEntries = FALSE;
	m_saved = TRUE;
	m_errorCode = 0;
	m_pFont = NULL;
	m_pDropTarget = NULL;
	m_pPane = pane;

	CABMLDialogEntryList *pInstance = new CABMLDialogEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );

	HandleErrorReturn(AB_InitializeMailingListPaneAB2(pane));

	ApiApiPtr(api);
	m_pUnkAddress = api->CreateClassInstance(
		APICLASS_ADDRESSCONTROL,NULL);
	m_pUnkAddress->QueryInterface(IID_IAddressControl,(LPVOID*)&m_pIAddressList);
	m_pIAddressList->SetContext(context);
    m_pIAddressList->SetControlParent(this);

	// these next three refer to flags around events that
	// we receive from m_pIAddressList.
	m_changingEntry = FALSE;
	m_addingEntry = FALSE;
	m_deletingEntry =FALSE;

	//{{AFX_DATA_INIT(CAddrDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CABMLDialog::~CABMLDialog ( )
{
    if(m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }
}

void CABMLDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddrList)
	DDX_Text(pDX, IDC_Description, m_description);
	DDX_Text(pDX, IDC_NAME, m_name);
	DDX_Text(pDX, IDC_NICKNAME, m_nickname);
	//}}AFX_DATA_MAP
}


void CABMLDialog::PostNcDestroy()
{
	delete this;

} // END OF	FUNCTION CABMLDialog::PostNcDestroy()


void CABMLDialog::OnDestroy( )
{
	CleanupOnClose();
	if (m_pFont)
		theApp.ReleaseAppFont(m_pFont);
}


void CABMLDialog::CleanupOnClose()
{
	if (m_pIAddrList)
		m_pIAddrList->Release();

	HandleErrorReturn(AB_ClosePane(m_pPane));
    
    m_pUnkAddress->Release();
}

void CABMLDialog::OnHelp()
{
	if (m_entryID == MSG_MESSAGEIDNONE)
		NetHelp(HELP_ADD_LIST_MAILING_LIST);
	else
		NetHelp(HELP_MAIL_LIST_PROPS);
}

void CABMLDialog::OnUpdateOnlineStatus(CCmdUI *pCmdUI)
{
 	pCmdUI->Enable(!NET_IsOffline());
}

/////////////////////////////////////////////////////////////////////////////
// CABMLDialog Overloaded methods
/////////////////////////////////////////////////////////////////////////////
BOOL CABMLDialog::OnInitDialog( )
{
	if (CDialog::OnInitDialog()) {
		CWnd* widget;
		CRect rect2, rect3;

		HDC hDC = ::GetDC(m_hWnd);
		LOGFONT lf;  
		memset(&lf,0,sizeof(LOGFONT));

		MWContext * context = FE_GetAddressBookContext (m_pPane, FALSE);

		lf.lfPitchAndFamily = FF_SWISS;
		lf.lfWeight = FW_NORMAL;
		lf.lfCharSet = IntlGetLfCharset(CIntlWin::GetSystemLocaleCsid());
		if (CIntlWin::GetSystemLocaleCsid() == CS_LATIN1)
 			_tcscpy(lf.lfFaceName, "MS Sans Serif");
		else
 			_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(CIntlWin::GetSystemLocaleCsid()));
		lf.lfHeight = -MulDiv(9, ::GetDeviceCaps(hDC, LOGPIXELSY), 72);
		m_pFont = theApp.CreateAppFont( lf );
		::SendMessage(::GetDlgItem(m_hWnd, IDC_NAME), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_NICKNAME), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_Description), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		::ReleaseDC(m_hWnd,hDC);

		MSG_SetFEData( m_pPane, (void *) m_pIAddrList );

		widget = GetDlgItem(IDC_ENTRIES);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(rect2);

		widget->DestroyWindow ();

		CRect rect(0,0,0,0);
        m_pIAddressList->Create(this,IDC_ADDRESSLIST);
		m_pIAddressList->AddAddressType(NULL);
		m_pIAddressList->SetDefaultBitmapId (IDB_PERSON);
   
        CListBox * pWnd = m_pIAddressList->GetListBox();
        pWnd->Invalidate();
        pWnd->MoveWindow(rect2.left, rect2.top, rect3.right, rect3.bottom, TRUE);
		m_pIAddressList->SetCSID(CIntlWin::GetSystemLocaleCsid());

		CPaintDC dc ( pWnd );      
		 
        pWnd->GetWindowRect(rect);    
        ScreenToClient(rect);    
        rect.InflateRect(4,4); 
        WFE_DrawRaisedRect(dc.GetSafeHdc(),rect);       

		widget = GetDlgItem(IDC_StatusRect);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		CRect windowRect, statusRect;
		GetClientRect(&windowRect);

		statusRect.SetRect(0, windowRect.Height() - rect2.Height(),
						   windowRect.Width(), windowRect.Height());

		widget->DestroyWindow ();

		// create the status bar
		UINT aIDArray[] = { IDS_TRANSFER_STATUS, ID_SEPARATOR, IDS_ONLINE_STATUS};

		m_barStatus.Create(this, FALSE, FALSE);
		m_barStatus.MoveWindow(&statusRect, TRUE);
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

		//move separator over status bar
		widget = GetDlgItem(IDC_STATUSSEPARATOR);
		widget->GetWindowRect(&rect2);
		widget->MoveWindow(0, statusRect.top - rect2.Height(),
						   windowRect.Width(), rect2.Height(), TRUE);
		AddEntriesToList (-1, -1);
		m_addingEntries = FALSE;
        
		AB_AttributeValue * values;
		AB_AttribID *attribs = new AB_AttribID[NUM_MAILING_LIST_ATTRIBUTES];

		uint16 numAttributes = NUM_MAILING_LIST_ATTRIBUTES;

		attribs[0] = AB_attribFullName;
		attribs[1] = AB_attribNickName;
		attribs[2] = AB_attribInfo;

		AB_GetMailingListAttributes(m_pPane, attribs, &values, &numAttributes);

		for(int i = 0; i < numAttributes; i++)
		{
			switch(values[i].attrib)
			{
				case AB_attribFullName:	m_name = XP_STRDUP(values[i].u.string);
										break;
				case AB_attribNickName: m_nickname = XP_STRDUP(values[i].u.string);
										break;
				case AB_attribInfo:		m_description = XP_STRDUP(values[i].u.string);
										break;
			}
		}

		delete attribs;

		AB_FreeEntryAttributeValues(values, numAttributes);

		UpdateData(FALSE);

		if(!m_pDropTarget) {
		   m_pDropTarget = new CMailListDropTarget(this);
		   m_pDropTarget->Register(this);
		}
		DragAcceptFiles();

	} else {
		return FALSE;
	}

	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CAddrDialog message handlers

BEGIN_MESSAGE_MAP(CABMLDialog, CDialog)
	//{{AFX_MSG_MAP(CABMLDialog)
	ON_BN_CLICKED( IDOK, OnOK)
	ON_BN_CLICKED( IDCANCEL, OnCancel)
	ON_BN_CLICKED( IDC_REMOVEITEM, OnRemoveEntry)
	ON_BN_CLICKED( ID_HELP, OnHelp)
	ON_UPDATE_COMMAND_UI(IDS_ONLINE_STATUS, OnUpdateOnlineStatus)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CABMLDialog::Create( LPCTSTR lpszTemplateName, CWnd* pParentWnd)
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		CDialog::Create (lpszTemplateName, pParentWnd);
		ret = TRUE;
	}
	return ret;
}

BOOL CABMLDialog::Create( UINT nIDTemplate, CWnd* pParentWnd)
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		CDialog::Create (nIDTemplate, pParentWnd);
		ret = TRUE;
	}
	return ret;
}

BOOL CABMLDialog::Create( CWnd *pParentWnd ) 
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		CDialog::Create (IDD, pParentWnd);
		ret = TRUE;
	}
	return ret;

}


int CABMLDialog::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	m_MailNewsResourceSwitcher.Reset();
	int res = CDialog::OnCreate(lpCreateStruct);

	MSG_SetFEData( (MSG_Pane*) m_pPane, (void *) m_pIAddrList );
	return res;
}


BOOL CABMLDialog::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN)
	{
        if (pMsg->wParam == VK_TAB)
		{
            BOOL bControl = GetKeyState(VK_CONTROL) & 0x8000;
            BOOL bShift = GetKeyState(VK_SHIFT) & 0x8000;
			CWnd* hwndNext = NULL;

			CWnd* hwndFocus = GetFocus();

			CWnd* hwndDescription = GetDlgItem( IDC_Description );
			CWnd* hwndEntries = m_pIAddressList->GetListBox();
			CEdit* hwndEntriesEdit = m_pIAddressList->GetAddressNameField();
			CWnd* hwndOK = GetDlgItem( IDOK );

			if ( bShift ) {
				// Tab backward
				if ( hwndFocus == hwndEntries || hwndFocus == hwndEntriesEdit ) {
					// Tab backward into description
					hwndNext = hwndDescription;	
					if (IsWindow(hwndEntriesEdit->m_hWnd)) {
				        hwndEntriesEdit->ShowWindow(SW_HIDE);
					}
				} else if (hwndFocus == hwndOK) {
					// Tab into list
					hwndNext = hwndEntries;
				}
			} else {
				// Tab forward
				if (hwndFocus == hwndDescription) {
					// Handle tabbing into list
					hwndNext = hwndEntries;
				} else if ( hwndFocus == hwndEntries || hwndFocus == hwndEntriesEdit ) {
					if (IsWindow(hwndEntriesEdit->m_hWnd)) {
				        hwndEntriesEdit->ShowWindow(SW_HIDE);
					}
					hwndNext = hwndOK;
				}
			}
			if ( hwndNext ) {
				hwndNext->SetFocus();
				return TRUE;
			}
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

void CABMLDialog::OnOK() 
{

	MWContext * context = FE_GetAddressBookContext (m_pPane, FALSE);
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);

	UpdateData();

	AB_AttributeValue *values = new AB_AttributeValue[NUM_MAILING_LIST_ATTRIBUTES + 1];

	values[0].attrib = AB_attribFullName;
	values[0].u.string = XP_STRDUP(m_name.GetBuffer(0));

	values[1].attrib = AB_attribNickName;
	values[1].u.string = XP_STRDUP(m_nickname.GetBuffer(0));

	values[2].attrib = AB_attribInfo;
	values[2].u.string = XP_STRDUP(m_description.GetBuffer(0));

	values[3].attrib  = AB_attribWinCSID;
	values[3].u.shortValue = INTL_GetCSIWinCSID(csi);

	AB_SetMailingListAttributes(m_pPane, values, NUM_MAILING_LIST_ATTRIBUTES + 1);
	
	for(int attribNum = 0; attribNum < NUM_MAILING_LIST_ATTRIBUTES + 1; attribNum++)
	{

		if(AB_IsStringEntryAttributeValue(&values[attribNum]))
		{
			XP_FREE(values[attribNum].u.string);
		}
	}

	delete values;


	//Remove all empty strings since we add them whenever inserting a 
	// new item
	char *szType = NULL , *szName = NULL;
	UINT idBitmap;
	unsigned long idEntry;

    CListBox * pListBox = m_pIAddressList->GetListBox();
    int count = pListBox->GetCount();

	for(int k = count - 1; k >= 0; k--)
	{
		if(m_pIAddressList->GetEntry( k,&szType, &szName, &idBitmap, &idEntry))
		{
			if(szName && strlen(szName) == 0)
			{
				AB_CommandAB2(m_pPane, AB_DeleteCmd, (MSG_ViewIndex*)&k , 1);
			}

		}
	}

	// since we've accepted all additions then commit the changes.
	//ensure that no list changes get through as well.
	m_changingEntry = TRUE;
	AB_CommitChanges(m_pPane);
	m_changingEntry = FALSE;
	CDialog::OnOK();	
	DestroyWindow();
}

void CABMLDialog::OnCancel() 
{
	CDialog::OnCancel();
	DestroyWindow();
}

void CABMLDialog::AddEntriesToList(int index, int num)
{
	uint32 count = 0;
	ABID entryID;
	AB_EntryType type = AB_Person;
	DIR_Server* pab = NULL;
	CListBox * pWnd = m_pIAddressList->GetListBox();

//	if (m_changingEntry)
//		return;


	if (m_pPane)
	{
		count = MSG_GetNumLines(m_pPane);

		if (count) {
			m_addingEntries = TRUE;
			//if m_addingEntry is TRUE, this means that m_pIAddressList has already added
			//an Entry so we don't need to add it again.
			if(!m_addingEntry && !m_changingEntry)
			{
				if (index != -1)
				{
					count = index + num;
					for (int i = index; i < count; i++) 
					{
						// add the entry to the list
						m_pIAddressList->InsertEntry(i, FALSE, "", "", IDB_PERSON, 0);
					}
				}
				else 
				{
	    			pWnd->ResetContent();
					index = 0;
					for (int i = 0; i < count; i++) 
					{
						// add the entry to the list
						m_pIAddressList->AppendEntry(FALSE, "", "", IDB_PERSON, 0);
					}
				}
			}

			uint16 numAttributes;
		
			AB_AttribID attribs[NUM_MAILING_LIST_ENTRY_ATTRIBUTES] = {AB_attribEntryType, AB_attribFullAddress};

			AB_AttributeValue *values;

			for (int j = index; j < count; j++) 
			{

				char *fullname = NULL;

				numAttributes = NUM_MAILING_LIST_ENTRY_ATTRIBUTES;
				HandleErrorReturn(AB_GetMailingListEntryAttributes(m_pPane, j, attribs, &values,
								  &numAttributes));

				for(int k = 0; k < numAttributes; k++)
				{
					switch(values[k].attrib)
					{
						case AB_attribEntryType:	type = values[k].u.entryType;
													break;
						case AB_attribFullAddress:	fullname = XP_STRDUP(values[k].u.string);
													break;
					}

				}

				if(numAttributes > 0)
				{
					AB_FreeEntryAttributeValues(values, numAttributes);
				}

				// add the entry to the list
				m_pIAddressList->SetEntry(j,
                    "",fullname,type==AB_MailingList?IDB_MAILINGLIST:IDB_PERSON,entryID);
				XP_FREEIF (fullname);
	
			}
			m_addingEntries = FALSE;
		}
		else {
			m_pIAddressList->AppendEntry();
		}
	}
	else {
		// add the entry to the list
		m_pIAddressList->AppendEntry();
	}
	
    pWnd->Invalidate();
    pWnd->UpdateWindow();
}

#ifdef XP_WIN16
	#define LPNMHDR NMHDR*
#endif

void CABMLDialog::OnRemoveEntry(void)
{
    ASSERT(m_pIAddressList);
    CListBox * pListBox = m_pIAddressList->GetListBox();
    if (pListBox->GetCount())
    {
        CListBox * pWnd = m_pIAddressList->GetListBox();

		if(pListBox->GetCount() == 1)
		{
			m_pIAddressList->RemoveSelection();
		}
		else
		{
			m_pIAddressList->RemoveSelection();
		}
    }
}

void CABMLDialog::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane ==  m_pPane ) {
		++m_iMysticPlane;
	}
}


void CABMLDialog::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	ABID entryID = MSG_MESSAGEIDNONE;
	if ( pane == m_pPane ) {
		switch ( notify ) {
		case MSG_NotifyNone:
			break;

		case MSG_NotifyInsertOrDelete:
			// this could happen from someone deleting one of the members from the
			// address book or an undo action adding a member back in
            {
				if (num > 0)
		    		AddEntriesToList(CASTINT(where), CASTINT(num));
				else
					AddEntriesToList(-1, -1);
			}
			break;

		case MSG_NotifyChanged:
			{
				uint16 numAttributes = NUM_MAILING_LIST_ENTRY_ATTRIBUTES;
		
				AB_AttribID attribs[NUM_MAILING_LIST_ENTRY_ATTRIBUTES] = {AB_attribEntryType, AB_attribFullName};

				AB_AttributeValue *values;

				char *fullname = NULL;
				AB_EntryType type = AB_Person;
				if (m_pPane)
				{
					HandleErrorReturn(AB_GetMailingListEntryAttributes(m_pPane, where, attribs, &values,
									  &numAttributes));

					for(int k = 0; k < numAttributes; k++)
					{
						switch(values[k].attrib)
						{
							case AB_attribEntryType:	type = values[k].u.entryType;
														break;
							case AB_attribFullName:		fullname = values[k].u.string;
														break;
						}

					}

					m_pIAddressList->SetItemBitmap(CASTINT(where), type==AB_MailingList?IDB_MAILINGLIST:IDB_PERSON);
					m_pIAddressList->SetItemEntryID(CASTINT(where), entryID);
					m_pIAddressList->SetItemName(CASTINT(where), fullname);
					XP_FREEIF (fullname);
				}
			}
			break;

		case MSG_NotifyAll:
		case MSG_NotifyScramble:
			AddEntriesToList(-1, -1);
			break;
		}

		if (( !--m_iMysticPlane && m_addrBookPane)) {
			CListBox * pWnd = m_pIAddressList->GetListBox();
			pWnd->Invalidate();
			pWnd->UpdateWindow();
		}
	}
}


void CABMLDialog::HandleErrorReturn(int errorid, CWnd* parent)
{	
	if (errorid) {
		CString s;
		HWND parentWnd = NULL;
		if (parent)
			parentWnd = parent->m_hWnd;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			::MessageBox(parentWnd, XP_GetString(errorid), s, MB_OK | MB_APPLMODAL);
	}
}


// IAddressParent stuff
// these functions are in compbar.cpp, they should be prototyped
// in some header file and placed somewhere more appropriate

extern "C" char * wfe_ExpandForNameCompletion(char * pString);
extern "C" char * wfe_ExpandName(char * pString, unsigned long* entryID, UINT* bitmapID);

int CABMLDialog::ChangedItem (char * pString, int index, HWND hwnd, char ** ppFullName, unsigned long* entryID, UINT* bitmapID)
{
	if (!m_addingEntries) {
		ABID type, field, tempID = MSG_MESSAGEIDNONE;
		char* fullname = NULL;
		m_saved = FALSE;


		m_changingEntry = TRUE;
		AB_NameCompletionCookie *pCookie = NULL;
		int nNumResults = -1;

		m_pIAddressList->GetNameCompletionCookieInfo(&pCookie, &nNumResults, index);
		if(pCookie && nNumResults == 1)
		{
			AB_AddNCEntryToMailingList(m_pPane, index, pCookie, TRUE);
		}
		else
		{
			char *szType;
			char *szName;
			UINT idBitmap;
			unsigned long idEntry;

			if(m_pIAddressList->GetEntry( index, &szType, &szName, &idBitmap, &idEntry))
			{
				AB_AddNakedEntryToMailingList(m_pPane, index, szName, TRUE);

			}



		}

	//	(*bitmapID) = IDB_PERSON; 
		m_changingEntry = FALSE;
	}
	return 0;
}

void CABMLDialog::DeletedItem (HWND hwnd, LONG id, int index)
{
	AB_CommandAB2(m_pPane, AB_DeleteCmd, (MSG_ViewIndex*)&index , 1);
//	PrintItems("DeletedItem");

}

void CABMLDialog::AddedItem (HWND hwnd, LONG id, int index)
{
	m_addingEntry = TRUE;
	AB_NameCompletionCookie *pCookie = NULL;
	int nNumResults = -1;

	m_pIAddressList->GetNameCompletionCookieInfo(&pCookie, &nNumResults, index);
	if(pCookie && nNumResults == 1)
	{
		AB_AddNCEntryToMailingList(m_pPane, index, pCookie, FALSE);
	}
	else
	{
		char *szType;
		char *szName;
		UINT idBitmap;
		unsigned long idEntry;

		if(m_pIAddressList->GetEntry( index, &szType, &szName, &idBitmap, &idEntry))
		{
			AB_AddNakedEntryToMailingList(m_pPane, index, szName, FALSE);

		}



	}
	m_addingEntry = FALSE;

//	AB_CommandAB2(m_pPane, AB_InsertLineCmd, (MSG_ViewIndex*)&index , 1);
//	PrintItems("AddedItem");

}

void CABMLDialog::PrintItems(LPSTR comment)
{
	int count = MSG_GetNumLines(m_pPane);

	uint16 numAttributes;

	AB_AttribID attribs[NUM_MAILING_LIST_ENTRY_ATTRIBUTES] = {AB_attribEntryType, AB_attribFullName};

	AB_AttributeValue *values;

	for (int j = 0; j < count; j++) 
	{

		char *fullname = NULL;

		numAttributes = NUM_MAILING_LIST_ENTRY_ATTRIBUTES;
		HandleErrorReturn(AB_GetMailingListEntryAttributes(m_pPane, j, attribs, &values,
						  &numAttributes));

		BOOL hasFullName = FALSE;
		for(int k = 0; k < numAttributes; k++)
		{
			switch(values[k].attrib)
			{
				case AB_attribEntryType:	//type = values[k].u.entryType;
											break;
				case AB_attribFullName:		if(fullname != NULL)
											{
												fullname = XP_STRDUP(values[k].u.string);
											}
											else
											{
												fullname = XP_STRDUP("");
											}
											TRACE("%s %d %s\n", comment, j, fullname);
											hasFullName = TRUE;
											break;
			}

		}
		if(!hasFullName)
		{
			TRACE("%s %d \n", comment, j);
		}

		if(numAttributes > 0)
		{
			AB_FreeEntryAttributeValues(values, numAttributes);
		}
		XP_FREEIF(fullname);
	}



}

char * CABMLDialog::NameCompletion(char * pString)
{
	ABID entryID = -1;
    ABID field = -1;
	DIR_Server* pab  = NULL;

	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
    if (pab != NULL && theApp.m_pABook)
    {
	AB_GetIDForNameCompletion(
	    theApp.m_pABook,
	    pab, 
	    &entryID,&field,(LPCTSTR)pString);
	if (entryID != -1)
	{
		    if (field == ABNickname) {
			    char szNickname[kMaxNameLength];
			    AB_GetNickname(
				    pab, 
				    theApp.m_pABook, entryID, szNickname);
			    if (strlen(szNickname))
				    return strdup(szNickname);
		    }
	    else {
			    char szFullname[kMaxFullNameLength];
			    AB_GetFullName(pab, theApp.m_pABook, 
				    entryID, szFullname);
			    if (strlen(szFullname))
				    return strdup(szFullname);
		    }
	    }
    }

	return NULL;
}

void CABMLDialog::StartNameCompletionSearch()
{
	m_barStatus.StartAnimation();

}

void CABMLDialog::StopNameCompletionSearch()
{
	m_barStatus.StopAnimation();

}

void CABMLDialog::SetProgressBarPercent(int32 lPercent)
{
	m_barStatus.SetPercentDone (lPercent);

}

void CABMLDialog::SetStatusText(const char* pMessage)
{
	m_barStatus.SetWindowText( pMessage );
}

CWnd *CABMLDialog::GetOwnerWindow()
{
	return this;
}

BOOL CABMLDialog::IsDragInListBox(CPoint *pPoint)
{
	CRect listRect;

    CListBox * pListBox = m_pIAddressList->GetListBox();
	pListBox->GetWindowRect(LPRECT(listRect));
	ScreenToClient(LPRECT(listRect));
	if (listRect.PtInRect(*pPoint))
		return TRUE;
	else
		return FALSE;
}

DROPEFFECT CABMLDialog::GetAddressBookIndexFormatDropEffect(COleDataObject *pDataObject)
{
	DROPEFFECT res = DROPEFFECT_NONE;
	HGLOBAL hContent;

		
	AB_ContainerInfo *pContInfo = AB_GetContainerForMailingList(m_pPane);
	hContent = pDataObject->GetGlobalData ( ::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT));

	AddressBookDragData *pDragData = (AddressBookDragData *) GlobalLock(hContent);

	AB_DragEffect requireEffect = AB_Require_Copy;
	if ((GetKeyState(VK_SHIFT) & 0x8000) && (GetKeyState(VK_CONTROL) & 0x8000))
		requireEffect = AB_Require_Copy;
	else if (GetKeyState(VK_SHIFT) & 0x8000)
		requireEffect = AB_Require_Move;
	else if (GetKeyState(VK_CONTROL) & 0x8000)
		requireEffect = AB_Require_Copy;

	AB_DragEffect effect = AB_DragEntriesIntoContainerStatus (
		pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
		pContInfo,
		requireEffect);        /* do you want to do a move? a copy? default drag? */

	if (effect == AB_Require_Copy) 
		res = DROPEFFECT_COPY; 
	else if (effect == AB_Require_Move) 
		res = DROPEFFECT_MOVE; 
	else 
		res = DROPEFFECT_NONE; 

	GlobalUnlock (hContent); //do we have to GlobalFree it too?

	return res;

}

BOOL CABMLDialog::ProcessAddressBookIndexFormat(COleDataObject *pDataObject, DROPEFFECT effect)
{
	HGLOBAL hContent;

    if(!pDataObject)
        return FALSE;

	AB_ContainerInfo *pContInfo = AB_GetContainerForMailingList(m_pPane);

	if (pDataObject->IsDataAvailable( ::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT) ))
		hContent = pDataObject->GetGlobalData (::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT));
//	else
//		hContent = pDataObject->GetGlobalData (m_cfAddrIndexes);
	AddressBookDragData *pDragData = (AddressBookDragData *) GlobalLock(hContent);

	AB_DragEffect requireEffect = AB_Require_Copy;
	if (effect & DROPEFFECT_MOVE)
		requireEffect = AB_Require_Move;
	else if (effect & DROPEFFECT_COPY)
		requireEffect = AB_Require_Copy;

	// call new drag drop calls that handle indexes
	AB_DragEntriesIntoContainer( pDragData->m_pane, pDragData->m_indices, pDragData->m_count,
		pContInfo, requireEffect);         /* copy or move? */

	return TRUE;
}


BOOL CABMLDialog::ProcessVCardData(COleDataObject * pDataObject, CPoint &point)
{
	UINT clipFormat, fromABFormat;
	BOOL retVal;
	CWnd * pWnd = GetFocus();
	ABID tempID = MSG_MESSAGEIDNONE;
	HGLOBAL hEntryIDs = NULL;
	LPSTR pEntryIDs = NULL;

	if (pDataObject->IsDataAvailable(
		clipFormat = ::RegisterClipboardFormat(vCardClipboardFormat))) 
	{
		if (pDataObject->IsDataAvailable(
			fromABFormat = ::RegisterClipboardFormat(ADDRESSBOOK_SOURCETARGET_FORMAT)))
		{
			hEntryIDs = pDataObject->GetGlobalData(fromABFormat);
			pEntryIDs = (LPSTR)GlobalLock(hEntryIDs);
			ASSERT(pEntryIDs);
		}
		HGLOBAL hAddresses = pDataObject->GetGlobalData(clipFormat);
		LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
		ASSERT(pAddresses);
		XP_List * pEntries;
		int32 iEntries;

		if (m_pIAddressList)
		{
			int itemNum = 0;
			char * szType = NULL;
			char * szName = NULL;
			char * pszType = NULL;
			CListBox * pListBox = m_pIAddressList->GetListBox();
			if (!AB_ConvertVCardsToExpandedName(theApp.m_pABook,pAddresses,&pEntries,&iEntries))
			{
				XP_List * node = pEntries;
				char *pTemp = NULL;

				if (pEntryIDs)	//from Address Book
					pTemp = pEntryIDs;

				for (int32 i = 0; i < iEntries+1; i++)
				{
					char * pString = (char *)node->object; 
					if (pString != NULL)
					{
						m_changingEntry = TRUE;
						int nIndex = pListBox->GetCount() - 1;
						if (pEntryIDs)	//from Address Book
						{
							char  szEntryID[20];
							ABID  entryID;
							ABID type;
							char *fullname = NULL;
							DIR_Server* pab = NULL;

							szEntryID[0] = '\0';
							int nPos = 0;
							while (pTemp[nPos] != '\0')
							{
								if (pTemp[nPos] == '-')
								{
									strncpy(szEntryID, pTemp, nPos);
									szEntryID[nPos] = '\0';
									pTemp = pTemp + nPos + 1;
									break;
								}
								else 
									nPos++;
							}
							entryID	= atol(szEntryID);

							DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
							m_errorCode = AB_AddIDToMailingListAt(m_addrBookPane, entryID, nIndex);
							// need to change the bitmap based on the type
							HandleErrorReturn (AB_GetType (pab, theApp.m_pABook, entryID, &type));
							// if the entry is null, use a null terminated string 
							HandleErrorReturn (AB_GetExpandedName (pab, 
								theApp.m_pABook, entryID, &fullname));
							m_pIAddressList->SetEntry(nIndex, "", fullname, type==ABTypeList?IDB_MAILINGLIST:IDB_PERSON, 0);
							m_pIAddressList->AppendEntry(FALSE);
							XP_FREEIF (fullname);
						}
						else
						{
							char* names = NULL;
							char* addresses = NULL;
							int num = 0;

							num = MSG_ParseRFC822Addresses(pString, &names, &addresses);

							if (num > 0)
							{
								XP_ASSERT(names);
								XP_ASSERT(addresses);

								PersonEntry person;
								person.Initialize();
								person.pGivenName = names;
								person.pEmailAddress = addresses;
								MWContext * context = FE_GetAddressBookContext ((MSG_Pane *) m_addrBookPane, FALSE);
								INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);
								person.WinCSID = INTL_GetCSIWinCSID(csi);
								m_errorCode = AB_AddPersonToMailingListAt(m_addrBookPane, &person, nIndex, &tempID);
								m_pIAddressList->InsertEntry(nIndex, FALSE, "", pString, IDB_PERSON, 0);
								m_pIAddressList->AppendEntry(FALSE, "", "", IDB_PERSON);

								XP_FREEIF(names);
								XP_FREEIF(addresses);
							}
						}
					}
					node = node->next;
					if (!node)
						break;
				}
				XP_ListDestroy(pEntries);
			}
			if (hEntryIDs)
				GlobalUnlock(hEntryIDs);
			GlobalUnlock(hAddresses);
			retVal = TRUE;
			m_changingEntry = FALSE;
		}
	}
	if (pWnd && ::IsWindow(pWnd->m_hWnd))
		pWnd->SetFocus();
	return retVal;
}

//////////////////////////////////////////////////////////////////////////////
// CMailListDropTarget

DROPEFFECT CMailListDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	if(pDataObject->IsDataAvailable(::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT)))
	{
		if(m_pOwner->IsDragInListBox(&point))
		{
			return m_pOwner->GetAddressBookIndexFormatDropEffect(pDataObject);
		}

	}

	if(pDataObject->IsDataAvailable(
      ::RegisterClipboardFormat(vCardClipboardFormat)))
	{
		if (m_pOwner->IsDragInListBox(&point))
			deReturn = DROPEFFECT_COPY;
	}
	return(deReturn);
} 

BOOL CMailListDropTarget::OnDrop
(CWnd * pWnd, COleDataObject * pDataObject, DROPEFFECT effect, CPoint point)
{
	if(pDataObject->IsDataAvailable(::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT)))
	{
		return m_pOwner->ProcessAddressBookIndexFormat(pDataObject, effect);
	}

	if (pDataObject->IsDataAvailable(::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		return m_pOwner->ProcessVCardData(pDataObject,point);
	}
	return FALSE;
}




#else // MOZ_NEWADDR

#include "addrfrm.h"
#include "template.h"
#include "wfemsg.h"
#include "msg_srch.h"
#include "dirprefs.h"
#include "compfrm.h"
#include "apiaddr.h"
#include "nethelp.h"	// help
#include "xp_help.h"
#include "intl_csi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
#include "xpgetstr.h"
extern int MK_ADDR_ADD_PERSON_TO_ABOOK;
extern int MK_UNABLE_TO_OPEN_ADDR_FILE;
};


/////////////////////////////////////////////////////////////////////////////
// CAddrDialogEntryList

class CABMLDialogEntryList: public IMsgList {

	CABMLDialog *m_pABMLDialog;
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

	CABMLDialogEntryList( CABMLDialog *pAddrDialog ) {
		m_ulRefCount = 0;
		m_pABMLDialog = pAddrDialog;
	}
};


STDMETHODIMP CABMLDialogEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CABMLDialogEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CABMLDialogEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CABMLDialogEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pABMLDialog) {
		m_pABMLDialog->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
	}
}

void CABMLDialogEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pABMLDialog) {
		m_pABMLDialog->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
	}
}

void CABMLDialogEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CABMLDialogEntryList::SelectItem( MSG_Pane* pane, int item )
{
}



/////////////////////////////////////////////////////////////////////////////
// CAddrDialog

CABMLDialog::CABMLDialog(DIR_Server* dir, CWnd* pParent, 
	ABID listID, MWContext* context)
	: CDialog(CABMLDialog::IDD, pParent)
{
	CString msg;

	//{{AFX_DATA_INIT(CAddrList)
	m_description = _T("");
	m_name = _T("");
	m_nickname = _T("");
	//}}AFX_DATA_INIT

	m_dir = dir;
	m_entryID = listID;
	m_addingEntries = FALSE;
	m_saved = TRUE;
	m_errorCode = 0;
	m_changingEntry = FALSE;
	m_pFont = NULL;
	m_pDropTarget = NULL;

	CABMLDialogEntryList *pInstance = new CABMLDialogEntryList( this );
	pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pIAddrList );

	HandleErrorReturn(AB_InitMailingListPane(&m_addrBookPane,
		&m_entryID,
		dir,
		theApp.m_pABook,
		context, WFE_MSGGetMaster(),
		ABFullName, TRUE), this); 

	ApiApiPtr(api);
	m_pUnkAddress = api->CreateClassInstance(
		APICLASS_ADDRESSCONTROL,NULL);
	m_pUnkAddress->QueryInterface(IID_IAddressControl,(LPVOID*)&m_pIAddressList);
    m_pIAddressList->SetControlParent(this);
	m_pIAddressList->SetContext(context);

	//{{AFX_DATA_INIT(CAddrDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CABMLDialog::~CABMLDialog ( )
{
    if(m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }
}

void CABMLDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddrList)
	DDX_Text(pDX, IDC_Description, m_description);
	DDX_Text(pDX, IDC_NAME, m_name);
	DDX_Text(pDX, IDC_NICKNAME, m_nickname);
	//}}AFX_DATA_MAP
}


void CABMLDialog::PostNcDestroy()
{
	delete this;

} // END OF	FUNCTION CABMLDialog::PostNcDestroy()


void CABMLDialog::OnDestroy( )
{
	CleanupOnClose();
	if (m_pFont)
		theApp.ReleaseAppFont(m_pFont);
}


void CABMLDialog::CleanupOnClose()
{
	if (m_pIAddrList)
		m_pIAddrList->Release();

	HandleErrorReturn(AB_CloseMailingListPane(&m_addrBookPane), this);
	((CAddrFrame*) GetParent())->CloseListProperties (this, m_entryID);
    
    m_pUnkAddress->Release();
}

void CABMLDialog::OnHelp()
{
	if (m_entryID == MSG_MESSAGEIDNONE)
		NetHelp(HELP_ADD_LIST_MAILING_LIST);
	else
		NetHelp(HELP_MAIL_LIST_PROPS);
}

/////////////////////////////////////////////////////////////////////////////
// CABMLDialog Overloaded methods
/////////////////////////////////////////////////////////////////////////////
BOOL CABMLDialog::OnInitDialog( )
{
	if (CDialog::OnInitDialog()) {
		CWnd* widget;
		CRect rect2, rect3;

		HDC hDC = ::GetDC(m_hWnd);
		LOGFONT lf;  
		memset(&lf,0,sizeof(LOGFONT));

		MWContext * context = FE_GetAddressBookContext ((MSG_Pane *) m_addrBookPane, FALSE);

		lf.lfPitchAndFamily = FF_SWISS;
		lf.lfWeight = FW_NORMAL;
		lf.lfCharSet = IntlGetLfCharset(CIntlWin::GetSystemLocaleCsid());
		if (CIntlWin::GetSystemLocaleCsid() == CS_LATIN1)
 			_tcscpy(lf.lfFaceName, "MS Sans Serif");
		else
 			_tcscpy(lf.lfFaceName, IntlGetUIPropFaceName(CIntlWin::GetSystemLocaleCsid()));
		lf.lfHeight = -MulDiv(9, ::GetDeviceCaps(hDC, LOGPIXELSY), 72);
		m_pFont = theApp.CreateAppFont( lf );
		::SendMessage(::GetDlgItem(m_hWnd, IDC_NAME), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_NICKNAME), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		::SendMessage(::GetDlgItem(m_hWnd, IDC_Description), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		::ReleaseDC(m_hWnd,hDC);

		MSG_SetFEData( (MSG_Pane*) m_addrBookPane, (void *) m_pIAddrList );

		widget = GetDlgItem(IDC_ENTRIES);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(rect2);

		widget->DestroyWindow ();

		CRect rect(0,0,0,0);
        m_pIAddressList->Create(this,IDC_ADDRESSLIST);
		m_pIAddressList->AddAddressType(NULL);
		m_pIAddressList->SetDefaultBitmapId (IDB_PERSON);
   
        CListBox * pWnd = m_pIAddressList->GetListBox();
        pWnd->Invalidate();
        pWnd->MoveWindow(rect2.left, rect2.top, rect3.right, rect3.bottom, TRUE);
		m_pIAddressList->SetCSID(CIntlWin::GetSystemLocaleCsid());

		CPaintDC dc ( pWnd );      
		 
        pWnd->GetWindowRect(rect);    
        ScreenToClient(rect);    
        rect.InflateRect(4,4); 
        WFE_DrawRaisedRect(dc.GetSafeHdc(),rect);       

		AddEntriesToList (-1, -1);
		m_addingEntries = FALSE;
            
		HandleErrorReturn (AB_GetFullName(m_dir, theApp.m_pABook, m_entryID, m_name.GetBuffer(kMaxFullNameLength)), this);
		m_name.ReleaseBuffer(-1);
		HandleErrorReturn (AB_GetNickname(m_dir, theApp.m_pABook, m_entryID, m_nickname.GetBuffer(kMaxNameLength)), this);
		m_nickname.ReleaseBuffer(-1);
		HandleErrorReturn (AB_GetInfo(m_dir, theApp.m_pABook, m_entryID, m_description.GetBuffer(kMaxInfo)), this);
		m_description.ReleaseBuffer(-1);
		UpdateData(FALSE);

		if(!m_pDropTarget) {
		   m_pDropTarget = new CMailListDropTarget(this);
		   m_pDropTarget->Register(this);
		}
		DragAcceptFiles();

	} else {
		return FALSE;
	}

	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CAddrDialog message handlers

BEGIN_MESSAGE_MAP(CABMLDialog, CDialog)
	//{{AFX_MSG_MAP(CABMLDialog)
	ON_BN_CLICKED( IDOK, OnOK)
	ON_BN_CLICKED( IDCANCEL, OnCancel)
	ON_BN_CLICKED( IDC_REMOVEITEM, OnRemoveEntry)
	ON_BN_CLICKED( ID_HELP, OnHelp)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CABMLDialog::Create( LPCTSTR lpszTemplateName, CWnd* pParentWnd)
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		CDialog::Create (lpszTemplateName, pParentWnd);
		ret = TRUE;
	}
	return ret;
}

BOOL CABMLDialog::Create( UINT nIDTemplate, CWnd* pParentWnd)
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		CDialog::Create (nIDTemplate, pParentWnd);
		ret = TRUE;
	}
	return ret;
}

BOOL CABMLDialog::Create( CWnd *pParentWnd ) 
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		CDialog::Create (IDD, pParentWnd);
		ret = TRUE;
	}
	return ret;

}

int CABMLDialog::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	m_MailNewsResourceSwitcher.Reset();
	int res = CDialog::OnCreate(lpCreateStruct);

	MSG_SetFEData( (MSG_Pane*) m_addrBookPane, (void *) m_pIAddrList );
	return res;
}


BOOL CABMLDialog::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN)
	{
        if (pMsg->wParam == VK_TAB)
		{
            BOOL bControl = GetKeyState(VK_CONTROL) & 0x8000;
            BOOL bShift = GetKeyState(VK_SHIFT) & 0x8000;
			CWnd* hwndNext = NULL;

			CWnd* hwndFocus = GetFocus();

			CWnd* hwndDescription = GetDlgItem( IDC_Description );
			CWnd* hwndEntries = m_pIAddressList->GetListBox();
			CEdit* hwndEntriesEdit = m_pIAddressList->GetAddressNameField();
			CWnd* hwndOK = GetDlgItem( IDOK );

			if ( bShift ) {
				// Tab backward
				if ( hwndFocus == hwndEntries || hwndFocus == hwndEntriesEdit ) {
					// Tab backward into description
					hwndNext = hwndDescription;	
					if (IsWindow(hwndEntriesEdit->m_hWnd)) {
				        hwndEntriesEdit->ShowWindow(SW_HIDE);
					}
				} else if (hwndFocus == hwndOK) {
					// Tab into list
					hwndNext = hwndEntries;
				}
			} else {
				// Tab forward
				if (hwndFocus == hwndDescription) {
					// Handle tabbing into list
					hwndNext = hwndEntries;
				} else if ( hwndFocus == hwndEntries || hwndFocus == hwndEntriesEdit ) {
					if (IsWindow(hwndEntriesEdit->m_hWnd)) {
				        hwndEntriesEdit->ShowWindow(SW_HIDE);
					}
					hwndNext = hwndOK;
				}
			}
			if ( hwndNext ) {
				hwndNext->SetFocus();
				return TRUE;
			}
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

void CABMLDialog::OnOK() 
{
	// TODO: Add your specialized code here and/or call the base class
	MailingListEntry		list;
	ABID					tempID = MSG_MESSAGEIDNONE;
	int						errorID = 0;
	MSG_ViewIndex			index = 0;
	uint32					count = 0;
	DIR_Server*				pab = NULL;
	uint					i = 0;

	MWContext * context = FE_GetAddressBookContext ((MSG_Pane *) m_addrBookPane, FALSE);
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);

	UpdateData();
	list.Initialize();
	list.pNickName = m_nickname.GetBuffer(0);
	list.pFullName = m_name.GetBuffer(0);
	list.pInfo = m_description.GetBuffer(0);
	list.WinCSID = INTL_GetCSIWinCSID(csi);

	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
	XP_ASSERT (pab);
	if (!pab)
		HandleErrorReturn(MK_UNABLE_TO_OPEN_ADDR_FILE, this);

	AB_GetEntryCountInMailingList(m_addrBookPane, &count);

	do
	{
		if ((errorID = AB_ModifyMailingListAndEntriesWithChecks(m_addrBookPane, &list,
			&index, i)) != 0) {
			if (MK_ADDR_ADD_PERSON_TO_ABOOK == errorID)
			{
				CString s;
				if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
				{
					char * fullname = NULL;
					m_pIAddressList->SetSel (index, TRUE);
					tempID = AB_GetEntryIDAt((AddressPane*) m_addrBookPane, index);
					AB_GetExpandedName (pab, 
						theApp.m_pABook, tempID, &fullname);
					char* tmp = PR_smprintf(XP_GetString (MK_ADDR_ADD_PERSON_TO_ABOOK),
										fullname);
					if (tmp && fullname)
					{
						if (::MessageBox(this->m_hWnd, tmp, s, MB_OKCANCEL | MB_APPLMODAL) != IDOK)
						{
							XP_FREEIF (tmp);
							XP_FREEIF (fullname);
							return;
						}
						else
							errorID = 0;
						i = index;
						if (i == count - 1)
						{
							errorID = AB_ModifyMailingListAndEntriesWithChecks(m_addrBookPane, &list,
								&index, MSG_VIEWINDEXNONE);
							HandleErrorReturn(errorID, this);
						}
					}
					XP_FREEIF (tmp);
					XP_FREEIF (fullname);
				}
			}
			else
			{
				m_pIAddressList->SetSel (index, TRUE);
				HandleErrorReturn(errorID, this);
				break;
			}
		}
		i++;
	} while (i < count && errorID == 0); 

	if (!errorID)
		DestroyWindow();
}

void CABMLDialog::OnCancel() 
{
	CDialog::OnCancel();
	DestroyWindow();
}

void CABMLDialog::AddEntriesToList(int index, int num)
{
	uint32 count = 0;
	ABID entryID;
	ABID type;
	DIR_Server* pab = NULL;
	CListBox * pWnd = m_pIAddressList->GetListBox();

	if (m_changingEntry)
		return;

	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);

	XP_ASSERT (pab);

	if (pab)
	{
		HandleErrorReturn (AB_GetEntryCountInMailingList(m_addrBookPane, &count), this);

		if (count) {
			m_addingEntries = TRUE;
			if (index != -1)
			{
				count = index + num;
				for (int i = index; i < (int) count; i++) 
				{
					// add the entry to the list
					m_pIAddressList->InsertEntry(i, FALSE, "", "", IDB_PERSON, 0);
				}
			}
			else 
			{
	    		pWnd->ResetContent();
				index = 0;
				for (int i = 0; i < (int) count; i++) 
				{
					// add the entry to the list
					m_pIAddressList->AppendEntry(FALSE, "", "", IDB_PERSON, 0);
				}
			}

			for (int j = index; j < (int) count; j++) 
			{
				if ((entryID = AB_GetEntryIDAt((AddressPane*) m_addrBookPane, j)) != MSG_MESSAGEIDNONE) 
				{
					char *fullname = NULL;
					// need to change the bitmap based on the type
					HandleErrorReturn (AB_GetType (pab, theApp.m_pABook, entryID, &type), this);
					// if the entry is null, use a null terminated string 
					HandleErrorReturn (AB_GetExpandedName (pab, 
						theApp.m_pABook, entryID, &fullname), this);
					// add the entry to the list
					m_pIAddressList->SetEntry(j,
                        "",fullname,type==ABTypeList?IDB_MAILINGLIST:IDB_PERSON,entryID);
					XP_FREEIF (fullname);
				}
			}
		}
		else {
			m_pIAddressList->AppendEntry();
		}
	}
	else {
		// add the entry to the list
		m_pIAddressList->AppendEntry();
	}
	
    pWnd->Invalidate();
    pWnd->UpdateWindow();
}

#ifdef XP_WIN16
	#define LPNMHDR NMHDR*
#endif

void CABMLDialog::OnRemoveEntry(void)
{
    ASSERT(m_pIAddressList);
    CListBox * pListBox = m_pIAddressList->GetListBox();
    if (pListBox->GetCount())
    {
        CListBox * pWnd = m_pIAddressList->GetListBox();
        m_pIAddressList->RemoveSelection();
    }
}

void CABMLDialog::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		++m_iMysticPlane;
	}
}


void CABMLDialog::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
										MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
										int32 num)
{
	ABID entryID = MSG_MESSAGEIDNONE;
	if ( pane == (MSG_Pane*) m_addrBookPane ) {
		switch ( notify ) {
		case MSG_NotifyNone:
			break;

		case MSG_NotifyInsertOrDelete:
			// this could happen from someone deleting one of the members from the
			// address book or an undo action adding a member back in
            {
				if (num > 0)
		    		AddEntriesToList(CASTINT(where), CASTINT(num));
				else
					AddEntriesToList(-1, -1);
			}
			break;

		case MSG_NotifyChanged:
			if ((entryID = AB_GetEntryIDAt((AddressPane*) m_addrBookPane, where)) != MSG_MESSAGEIDNONE) 
			{
				char *fullname = NULL;
				DIR_Server* pab  = NULL;
				ABID type;
				DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
				XP_ASSERT (pab);
				if (pab)
				{
					HandleErrorReturn (AB_GetExpandedName (pab, 
						theApp.m_pABook, entryID, &fullname), this);
					HandleErrorReturn (AB_GetType (pab, theApp.m_pABook, entryID, &type), this);
					m_pIAddressList->SetItemBitmap(CASTINT(where), type==ABTypeList?IDB_MAILINGLIST:IDB_PERSON);
					m_pIAddressList->SetItemEntryID(CASTINT(where), entryID);
					m_pIAddressList->SetItemName(CASTINT(where), fullname);
					XP_FREEIF (fullname);
				}
			}
			break;

		case MSG_NotifyAll:
		case MSG_NotifyScramble:
			AddEntriesToList(-1, -1);
			break;
		}

		if (( !--m_iMysticPlane && m_addrBookPane)) {
			CListBox * pWnd = m_pIAddressList->GetListBox();
			pWnd->Invalidate();
			pWnd->UpdateWindow();
		}
	}
}


void CABMLDialog::HandleErrorReturn(int errorid, CWnd* parent)
{	
	if (errorid) {
		CString s;
		HWND parentWnd = NULL;
		if (parent)
			parentWnd = parent->m_hWnd;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			::MessageBox(parentWnd, XP_GetString(errorid), s, MB_OK | MB_APPLMODAL);
	}
}


// IAddressParent stuff
// these functions are in compbar.cpp, they should be prototyped
// in some header file and placed somewhere more appropriate

extern "C" char * wfe_ExpandForNameCompletion(char * pString);
extern "C" char * wfe_ExpandName(char * pString, unsigned long* entryID, UINT* bitmapID);

int CABMLDialog::ChangedItem (char * pString, int index, HWND hwnd, char ** ppFullName, unsigned long* entryID, UINT* bitmapID)
{
	if (!m_addingEntries) {
		ABID type, field, tempID = MSG_MESSAGEIDNONE;
		char* fullname = NULL;
		m_saved = FALSE;
		int errorID = 0;

		DIR_Server* pab  = NULL;

		DIR_GetPersonalAddressBook (theApp.m_directories, &pab);

		XP_ASSERT (pab);
		tempID = (*entryID);

		if (pab)
		{
			// check if this is one that has already been
			// expanded and hasn't changed
			if (tempID != MSG_MESSAGEIDNONE) {
				AB_GetExpandedName (pab, 
					theApp.m_pABook, tempID, &fullname);
				if (fullname)
				{
					if (XP_STRCMP (fullname, pString) == 0)
					{
						(*ppFullName) = fullname;
						return 0;
					}
				}
			}

			m_changingEntry = TRUE;

			AB_GetIDForNameCompletion(
				theApp.m_pABook,
				pab, 
				&tempID, &field, (LPCTSTR)pString);
		
			if (tempID != MSG_MESSAGEIDNONE)
			{			
				if ((*entryID) == MSG_MESSAGEIDNONE)
					m_errorCode = AB_AddIDToMailingListAt (m_addrBookPane, tempID, index);
				else
					m_errorCode = AB_ReplaceIDInMailingListAt(m_addrBookPane, tempID, index);

				AB_GetExpandedName (pab, 
					theApp.m_pABook, tempID, &fullname);
				// need to change the bitmap based on the type
				AB_GetType (pab, 
					theApp.m_pABook, tempID, &type);
				if (type == ABTypeList)
					(*bitmapID) = IDB_MAILINGLIST;          
				else
					(*bitmapID) = IDB_PERSON; 
			}
			else {
				PersonEntry person;
				person.Initialize();
				person.pGivenName = pString;
				person.pEmailAddress = pString;
				MWContext * context = FE_GetAddressBookContext ((MSG_Pane *) m_addrBookPane, FALSE);
				INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);
				person.WinCSID = INTL_GetCSIWinCSID(csi);

				m_errorCode = AB_AddPersonToMailingListAt(m_addrBookPane, &person, index, &tempID);
				fullname = (char *) XP_ALLOC (kMaxFullNameLength);
				if (fullname)
				{
					XP_STRCPY (fullname, "");
					XP_STRCPY (fullname, pString);
				}
				(*bitmapID) = IDB_PERSON; 
			}  
			m_changingEntry = FALSE;
			(*ppFullName) = fullname;
			(*entryID) = tempID;
			return 0;
		}
		else
			return 0;
	}
	return 0;
}

void CABMLDialog::DeletedItem (HWND hwnd, LONG id, int index)
{
	HandleErrorReturn (AB_RemoveIDFromMailingListAt (m_addrBookPane, index), this);
}

void CABMLDialog::AddedItem (HWND hwnd, LONG id, int index)
{
}

char * CABMLDialog::NameCompletion(char * pString)
{
	ABID entryID = -1;
    ABID field = -1;
	DIR_Server* pab  = NULL;

	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
    if (pab != NULL && theApp.m_pABook)
    {
	AB_GetIDForNameCompletion(
	    theApp.m_pABook,
	    pab, 
	    &entryID,&field,(LPCTSTR)pString);
	if (entryID != -1)
	{
		    if (field == ABNickname) {
			    char szNickname[kMaxNameLength];
			    AB_GetNickname(
				    pab, 
				    theApp.m_pABook, entryID, szNickname);
			    if (strlen(szNickname))
				    return strdup(szNickname);
		    }
	    else {
			    char szFullname[kMaxFullNameLength];
			    AB_GetFullName(pab, theApp.m_pABook, 
				    entryID, szFullname);
			    if (strlen(szFullname))
				    return strdup(szFullname);
		    }
	    }
    }

	return NULL;
}

BOOL CABMLDialog::IsDragInListBox(CPoint *pPoint)
{
	CRect listRect;

    CListBox * pListBox = m_pIAddressList->GetListBox();
	pListBox->GetWindowRect(LPRECT(listRect));
	ScreenToClient(LPRECT(listRect));
	if (listRect.PtInRect(*pPoint))
		return TRUE;
	else
		return FALSE;
}

BOOL CABMLDialog::ProcessVCardData(COleDataObject * pDataObject, CPoint &point)
{
	UINT clipFormat, fromABFormat;
	BOOL retVal;
	CWnd * pWnd = GetFocus();
	ABID tempID = MSG_MESSAGEIDNONE;
	HGLOBAL hEntryIDs = NULL;
	LPSTR pEntryIDs = NULL;
	DIR_Server* pab = NULL;

	if (pDataObject->IsDataAvailable(
		clipFormat = ::RegisterClipboardFormat(vCardClipboardFormat))) 
	{
		if (pDataObject->IsDataAvailable(
			fromABFormat = ::RegisterClipboardFormat(ADDRESSBOOK_SOURCETARGET_FORMAT)))
		{
			CAddrFrame* frame = (CAddrFrame*) GetParent();
			if (frame)
				pab = frame->GetCurrentDirectoryServer();
			if (DIR_AreServersSame (pab, m_dir))
			{
				hEntryIDs = pDataObject->GetGlobalData(fromABFormat);
				pEntryIDs = (LPSTR)GlobalLock(hEntryIDs);
				ASSERT(pEntryIDs);
			}
		}
		HGLOBAL hAddresses = pDataObject->GetGlobalData(clipFormat);
		LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
		ASSERT(pAddresses);
		XP_List * pEntries;
		int32 iEntries;

		if (m_pIAddressList)
		{
			int itemNum = 0;
			char * szType = NULL;
			char * szName = NULL;
			char * pszType = NULL;
			CListBox * pListBox = m_pIAddressList->GetListBox();
			if (!AB_ConvertVCardsToExpandedName(theApp.m_pABook,pAddresses,&pEntries,&iEntries))
			{
				XP_List * node = pEntries;
				char *pTemp = NULL;

				if (pEntryIDs)	//from Address Book
					pTemp = pEntryIDs;

				for (int32 i = 0; i < iEntries+1; i++)
				{
					char * pString = (char *)node->object; 
					if (pString != NULL)
					{
						m_changingEntry = TRUE;
						uint32 count = 0;
						AB_GetEntryCountInMailingList(m_addrBookPane, &count);
						int nIndex = count;
						if (pEntryIDs)	//from Address Book
						{
							char  szEntryID[20];
							ABID  entryID;
							ABID type;
							char *fullname = NULL;
							pab = NULL;

							szEntryID[0] = '\0';
							int nPos = 0;
							while (pTemp[nPos] != '\0')
							{
								if (pTemp[nPos] == '-')
								{
									strncpy(szEntryID, pTemp, nPos);
									szEntryID[nPos] = '\0';
									pTemp = pTemp + nPos + 1;
									break;
								}
								else 
									nPos++;
							}
							entryID	= atol(szEntryID);

							DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
							m_errorCode = AB_AddIDToMailingListAt(m_addrBookPane, entryID, nIndex);
							// need to change the bitmap based on the type
							HandleErrorReturn (AB_GetType (pab, theApp.m_pABook, entryID, &type));
							// if the entry is null, use a null terminated string 
							HandleErrorReturn (AB_GetExpandedName (pab, 
								theApp.m_pABook, entryID, &fullname));
							m_pIAddressList->SetEntry(nIndex, "", fullname, type==ABTypeList?IDB_MAILINGLIST:IDB_PERSON, 0);
							m_pIAddressList->AppendEntry(FALSE);
							XP_FREEIF (fullname);
						}
						else
						{
							char* names = NULL;
							char* addresses = NULL;
							int num = 0;

							num = MSG_ParseRFC822Addresses(pString, &names, &addresses);

							if (num > 0)
							{
								XP_ASSERT(names);
								XP_ASSERT(addresses);

								PersonEntry person;
								person.Initialize();
								person.pGivenName = names;
								person.pEmailAddress = addresses;
								MWContext * context = FE_GetAddressBookContext ((MSG_Pane *) m_addrBookPane, FALSE);
								INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);
								person.WinCSID = INTL_GetCSIWinCSID(csi);
								m_errorCode = AB_AddPersonToMailingListAt(m_addrBookPane, &person, nIndex, &tempID);
								m_pIAddressList->InsertEntry(nIndex, FALSE, "", pString, IDB_PERSON, 0);
								m_pIAddressList->AppendEntry(FALSE, "", "", IDB_PERSON);

								XP_FREEIF(names);
								XP_FREEIF(addresses);
							}
						}
					}
					node = node->next;
					if (!node)
						break;
				}
				XP_ListDestroy(pEntries);
			}
			if (hEntryIDs)
				GlobalUnlock(hEntryIDs);
			GlobalUnlock(hAddresses);
			retVal = TRUE;
			m_changingEntry = FALSE;
		}
	}
	if (pWnd && ::IsWindow(pWnd->m_hWnd))
		pWnd->SetFocus();
	return retVal;
}

//////////////////////////////////////////////////////////////////////////////
// CMailListDropTarget

DROPEFFECT CMailListDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	// Only interested in vcard
	if(pDataObject->IsDataAvailable(
      ::RegisterClipboardFormat(vCardClipboardFormat)))
	{
		if (m_pOwner->IsDragInListBox(&point))
			deReturn = DROPEFFECT_COPY;
	}
	return(deReturn);
} 

BOOL CMailListDropTarget::OnDrop
(CWnd * pWnd, COleDataObject * pDataObject, DROPEFFECT, CPoint point)
{
	if (pDataObject->IsDataAvailable(::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		return m_pOwner->ProcessVCardData(pDataObject,point);
	}
	return FALSE;
}

#endif //MOZ_NEWADDR
