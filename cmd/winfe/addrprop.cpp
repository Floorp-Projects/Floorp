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

// addrprop.cpp : implementation file for property dialogs from
// the address book
//
#include "stdafx.h"
#include "rosetta.h"
#include "addrprop.h"
#include "msgnet.h"


#ifdef MOZ_NEWADDR

#include "addrfrm.h"
#include "template.h"
#include "xpgetstr.h"
#include "wfemsg.h"
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

extern "C" {
#include "xpgetstr.h"
extern int MK_ADDR_BOOK_CARD;

#define NUM_ADDRESS_USER_ATTRIBUTES 9
#define NUM_ADDRESS_CONTACT_ATTRIBUTES 11

BOOL IsNumeric(char* pStr);
};

////////////////////////////////////////////////////////////////////////////
// CAddrEditProperities

CAddrEditProperties::CAddrEditProperties (CAddrFrame* frameref,
											LPCTSTR lpszCaption, CWnd * parent,
											MSG_Pane* pane,
											MWContext* context, BOOL bNew) :
    CNetscapePropertySheet ( lpszCaption, parent )
{
	m_pUserProperties = NULL;
	m_pContact = NULL;
	m_pSecurity = NULL;
	m_pCooltalk = NULL;
	m_context = context;
	m_pPane = pane;
	m_frame = frameref;
	m_bNew = bNew;
}

CAddrEditProperties::~CAddrEditProperties ( )
{
	if (m_pFont) {
		theApp.ReleaseAppFont(m_pFont);
	}
	if ( m_pUserProperties )
        delete m_pUserProperties;
	if ( m_pContact )
        delete m_pContact;
	if ( m_pSecurity )
        delete m_pSecurity;
	if ( m_pCooltalk )
        delete m_pCooltalk;
}

BOOL CAddrEditProperties::Create(CWnd* pParentWnd, DWORD dwStyle, DWORD dwExStyle)
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		m_pUserProperties = new CAddressUser (this, m_bNew);
		m_pContact = new CAddressContact (this);
		m_pSecurity = NULL;
		m_pCooltalk = new CAddressCooltalk (this);
		AddPage( m_pUserProperties );
		AddPage( m_pContact );
		AddPage( m_pCooltalk );
		SetAttributes(m_pPane);
		CNetscapePropertySheet::Create (pParentWnd, dwStyle, dwExStyle);
		ret = TRUE;
	}
	return ret;
}


int CAddrEditProperties::DoModal()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;
	m_pUserProperties = new CAddressUser (this, m_bNew);
	m_pContact = new CAddressContact (this);
	m_pSecurity = NULL;
	m_pCooltalk = new CAddressCooltalk (this);
	AddPage( m_pUserProperties );
	AddPage( m_pContact );
	AddPage( m_pCooltalk );
	SetAttributes(m_pPane);
	return CNetscapePropertySheet::DoModal();
}

void CAddrEditProperties::OnHelp()
{
	if (m_entryID == 0)
	{
		if (GetActivePage() == m_pUserProperties)
			NetHelp(HELP_ADD_USER_PROPS);
		else if (GetActivePage() == m_pContact)
			NetHelp(HELP_ADD_USER_CONTACT);
		HG21511
		else if (GetActivePage() == m_pCooltalk)
			NetHelp(HELP_ADD_USER_NETSCAPE_COOLTALK);
	}
	else
	{
		if (GetActivePage() == m_pUserProperties)
			NetHelp(HELP_EDIT_USER);
		else if (GetActivePage() == m_pContact)
			NetHelp(HELP_EDIT_USER_CONTACT);
		HG19721
		else if (GetActivePage() == m_pCooltalk)
			NetHelp(HELP_EDIT_USER_CALLPOINT);
	}
}

BEGIN_MESSAGE_MAP(CAddrEditProperties, CNetscapePropertySheet)
	//{{AFX_MSG_MAP(CAddrEditProperties)
		// NOTE: the ClassWizard will add message map macros here
		ON_BN_CLICKED(IDC_TO, CloseWindow)
	    ON_WM_CREATE()
		ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CAddrEditProperties::SetAttributes(MSG_Pane* pane)
{
	// otherwise just switch the contents of the sheet
	((CAddressUser*)m_pUserProperties)->SetAttributes(pane);
	((CAddressContact*)m_pContact)->SetAttributes(pane);
	((CAddressCooltalk*)m_pCooltalk)->SetAttributes(pane);
}

int CAddrEditProperties::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	m_MailNewsResourceSwitcher.Reset();
	if (CNetscapePropertySheet::OnCreate(lpCreateStruct) == -1)
		return -1;
#ifdef XP_WIN16
	OnInitDialog();
#endif
	return 0;
}

static int rgiButtons[] = { IDOK, IDC_TO, IDCANCEL, ID_APPLY_NOW, IDHELP };



BOOL CAddrEditProperties::OnInitDialog ( )
{            
	BOOL bResult = TRUE;
	CString label;   

	m_MailNewsResourceSwitcher.Reset();
	int16 guicsid = 0;
	if (m_context)
	{
		INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_context);
		guicsid = INTL_GetCSIWinCSID(csi);
	}	
	else
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
		

	bResult = CNetscapePropertySheet::OnInitDialog();

	if (m_bModeless)
	{
		// layout property sheet so button area IS accounted for
		CRect rectWnd;
		GetWindowRect(rectWnd);
		CRect rectButton;
		HWND hWnd = ::GetDlgItem(m_hWnd, IDOK);
		ASSERT(hWnd != NULL);
		::GetWindowRect(hWnd, rectButton);

		SetWindowPos(NULL, 0, 0,
			rectWnd.Width(), rectButton.bottom - rectWnd.top + 8,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

		// We don't use apply
		CWnd* hWnd2 = GetDlgItem(ID_APPLY_NOW);
		if (hWnd2)
			hWnd2->DestroyWindow ();

		CWnd* widget;
		CRect rect2, rect3;

		// create the CLOSE button
		widget = GetDlgItem(IDCANCEL);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		widget->DestroyWindow ();

		CButton * pButton = new CButton;
		label.LoadString (IDS_CANCEL_BUTTON);
		pButton->Create( label, 
			WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			rect3, this, IDC_TO);
		pButton->MoveWindow(&rect2, TRUE);
		pButton->SetFont(GetDlgItem(IDOK)->GetFont(), TRUE);

		// readd some of the standard buttons for modeless dialogs
		for (int i = 0; i < sizeof(rgiButtons)/sizeof(rgiButtons[0]); i++)
		{
			HWND hWnd = ::GetDlgItem(m_hWnd, rgiButtons[i]);
			if (hWnd != NULL)
			{
				::ShowWindow(hWnd, SW_SHOW);
				::EnableWindow(hWnd, TRUE);
			}
		}
	}

	return bResult;
}

void CAddrEditProperties::OnDestroy( )
{

	CAddrFrame::HandleErrorReturn(AB_ClosePane(m_pPane));


}


void CAddrEditProperties::CloseWindow()
{
	CButton * pButton = NULL;
	pButton = (CButton *) GetDlgItem (IDC_TO);
	if (pButton)
		delete pButton;

#ifdef XP_WIN16
	if (m_bModeless)
	{
		pButton = (CButton *) GetDlgItem (IDHELP);
		if (pButton)
			delete pButton;
		pButton = (CButton *) GetDlgItem (IDOK);
		if (pButton)
			delete pButton;
	}
#endif
#ifdef XP_WIN32
	if (m_bModeless)
		OnClose();  
	else
		EndDialog(IDOK);
#else
	if (m_bModeless)
		DestroyWindow();
	else
		EndDialog (IDOK);
#endif

}

/****************************************************************************
*
*	CAddrEditProperties::OnOK
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We override this function because we are a modeless window.
*
****************************************************************************/

void CAddrEditProperties::OnOK()
{
	PersonEntry		person;
	int				errorID = 0;

	// add or modify a user
	person.Initialize();

	((CAddressUser*)m_pUserProperties)->PerformOnOK(m_pPane);
	((CAddressContact*)m_pContact)->PerformOnOK(m_pPane);
	((CAddressCooltalk*)m_pCooltalk)->PerformOnOK(m_pPane);

	AB_CommitChanges(m_pPane);

	// if this is a new user then we will need to create an entry
/*	if (GetEntryID() == NULL) {
		ABID			entryID;
		if ((errorID = AB_AddUser (m_dir, theApp.m_pABook, &person, &entryID)) != 0) {
			CString s;
			CAddrFrame::HandleErrorReturn(errorID, GetParent());
			SetCurrentPage(0);
			m_pUserProperties->GetDlgItem(IDC_FIRSTNAME)->SetFocus();
			return;
		}
	}
	else {
		// we are potentially modifying a user so we will edit them
		if ((errorID = AB_ModifyUser(m_dir, theApp.m_pABook, GetEntryID(), &person)) != 0) {
			CString s;
			CAddrFrame::HandleErrorReturn(errorID, GetParent());
			SetCurrentPage(0);
			m_pUserProperties->GetDlgItem(IDC_FIRSTNAME)->SetFocus();
			return;
		}
	}
*/	
	CButton * pButton = NULL;
	pButton = (CButton *) GetDlgItem (IDC_TO);
	if (pButton)
		delete pButton;

#ifdef XP_WIN16
	if (m_bModeless)
	{
		pButton = (CButton *) GetDlgItem (IDHELP);
		if (pButton)
			delete pButton;
		pButton = (CButton *) GetDlgItem (IDOK);
		if (pButton)
			delete pButton;
	}
#endif

	if (m_bModeless)
		DestroyWindow();
	else
		EndDialog (IDOK);


} // END OF	FUNCTION CAddrEditProperities::OnOK()


/////////////////////////////////////////////////////////////////////////////
// CAddressUser property page

CAddressUser::CAddressUser(CWnd *pParent, BOOL bNew) 
	: CNetscapePropertyPage(CAddressUser::IDD)
{
	//{{AFX_DATA_INIT(CAddressUser)
	m_address = _T("");
	m_description = _T("");
	m_firstname = _T("");
	m_lastname = _T("");
	m_nickname = _T("");
	m_useHTML = 0;
	m_company = _T("");
	m_title = _T("");
	m_department = _T("");
	m_displayname = _T("");
	//}}AFX_DATA_INIT
	m_bActivated = FALSE;
	m_bNew = bNew;	//is this a new card
	m_bUserChangedDisplay = FALSE; //has the user typed in the display name field yet.
}

CAddressUser::~CAddressUser()
{
}

void CAddressUser::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddressUser)
	DDX_Text(pDX, IDC_COMPANY_NAME, m_company);
	DDX_Text(pDX, IDC_TITLE, m_title);
	DDX_Text(pDX, IDC_ADDRESS, m_address);
	DDX_Text(pDX, IDC_Description, m_description);
	DDX_Text(pDX, IDC_FIRSTNAME, m_firstname);
	DDX_Text(pDX, IDC_LASTNAME, m_lastname);
	DDX_Text(pDX, IDC_NICKNAME, m_nickname);
	DDX_Check(pDX, IDC_CHECK1, m_useHTML);
	DDX_Text(pDX, IDC_DEPARTMENT, m_department);
	DDX_Text(pDX, IDC_DISPLAYNAME, m_displayname);
	//}}AFX_DATA_MAP
}

void CAddressUser::SetFonts(HFONT pFont)
{	
	::SendMessage(::GetDlgItem(m_hWnd, IDC_COMPANY_NAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_TITLE), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ADDRESS), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_Description), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_FIRSTNAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_LASTNAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_NICKNAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_DEPARTMENT), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_DISPLAYNAME), WM_SETFONT, (WPARAM)pFont, FALSE);

}


BEGIN_MESSAGE_MAP(CAddressUser, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CAddressUser)
		// NOTE: the ClassWizard will add message map macros here
		ON_BN_CLICKED(IDC_TO, OnCloseWindow)
		ON_EN_CHANGE( IDC_FIRSTNAME, OnNameTextChange )
		ON_EN_CHANGE( IDC_LASTNAME, OnNameTextChange)
		ON_EN_CHANGE( IDC_DISPLAYNAME, OnDisplayNameTextChange)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddressUser message handlers

void CAddressUser::OnNameTextChange()
{

	if(m_bNew && !m_bUserChangedDisplay)
	{
		UpdateData(TRUE);

		char *displayName = NULL;
		AB_GenerateDefaultDisplayName(m_firstname, m_lastname, &displayName);
		if(displayName)
		{
			m_displayname = displayName;
			UpdateData(FALSE);
			XP_FREE(displayName);
		}
	}
}

void CAddressUser::OnDisplayNameTextChange()
{

	if(m_bNew)
	{
		m_bUserChangedDisplay = TRUE;
	}
}

BOOL CAddressUser::OnInitDialog() 
{
	// TODO: Add your specialized code here and/or call the base class
	CNetscapePropertyPage::OnInitDialog();
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());
	return TRUE;
}

void CAddressUser::SetAttributes (MSG_Pane *pane)
{

	uint16 numAttributes = NUM_ADDRESS_USER_ATTRIBUTES;

	AB_AttribID* attribs = new AB_AttribID[NUM_ADDRESS_USER_ATTRIBUTES];

	attribs[0] = AB_attribGivenName;
	attribs[1] = AB_attribNickName;
	attribs[2] = AB_attribFamilyName;
	attribs[3] = AB_attribEmailAddress;
	attribs[4] = AB_attribCompanyName;
	attribs[5] = AB_attribTitle;
	attribs[6] = AB_attribInfo;
	attribs[7] = AB_attribHTMLMail;
	attribs[8] = AB_attribDisplayName;

	AB_AttributeValue* values;

	AB_GetPersonEntryAttributes(pane, attribs, &values, &numAttributes);

	for(int i = 0; i < numAttributes; i++)
	{
		switch(values[i].attrib)
		{
			case AB_attribGivenName:    m_firstname = values[i].u.string;
									    break;
			case AB_attribNickName:     m_nickname = values[i].u.string;
									    break;
			case AB_attribFamilyName:   m_lastname = values[i].u.string;
									    break;
			case AB_attribEmailAddress: m_address = values[i].u.string;
									    break;
			case AB_attribCompanyName:  m_company = values[i].u.string;
										break;
			case AB_attribTitle:		m_title = values[i].u.string;
										break;
			case AB_attribInfo:			m_description = values[i].u.string;
										break;
			case AB_attribHTMLMail:		m_useHTML = values[i].u.boolValue;
										break;
			case AB_attribDisplayName:  m_displayname = values[i].u.string;
										break;
		}



	}

	AB_FreeEntryAttributeValues(values, numAttributes);

/*	// if it is an entry that hasn't been added to the database yet then return
	if (person) {
		if (person->pGivenName)
			m_firstname = person->pGivenName;
		if (person->pNickName)
			m_nickname = person->pNickName;
		if (person->pFamilyName)
			m_lastname = person->pFamilyName;
		if (person->pEmailAddress)
			m_address = person->pEmailAddress;
		if (person->pCompanyName)
			m_company = person->pCompanyName;
		if (person->pTitle)
			m_title = person->pTitle;
		if (person->pInfo)
			m_description = person->pInfo;
		if (person->HTMLmail)
			m_useHTML = person->HTMLmail;
		return;
	}

	if (entryID) {
		// otherwise update the fields
		AB_GetGivenName(m_dir, theApp.m_pABook, entryID, m_firstname.GetBuffer(kMaxNameLength));
		m_firstname.ReleaseBuffer(-1);
		AB_GetNickname(m_dir, theApp.m_pABook, entryID, m_nickname.GetBuffer(kMaxNameLength));
		m_nickname.ReleaseBuffer(-1);
		AB_GetFamilyName(m_dir, theApp.m_pABook, entryID, m_lastname.GetBuffer(kMaxNameLength));
		m_lastname.ReleaseBuffer(-1);
		AB_GetEmailAddress(m_dir, theApp.m_pABook, entryID, m_address.GetBuffer(kMaxEmailAddressLength));
		m_address.ReleaseBuffer(-1);
		AB_GetInfo(m_dir, theApp.m_pABook, entryID, m_description.GetBuffer(kMaxInfo));
		m_description.ReleaseBuffer(-1);
		XP_Bool useHTML = FALSE;
		AB_GetHTMLMail(m_dir, theApp.m_pABook, entryID, &useHTML);
		m_useHTML = useHTML;
		AB_GetCompanyName(m_dir, theApp.m_pABook, entryID, m_company.GetBuffer(kMaxCompanyLength));
		m_company.ReleaseBuffer(-1);
		AB_GetTitle(m_dir, theApp.m_pABook, entryID, m_title.GetBuffer(kMaxTitle));
		m_title.ReleaseBuffer(-1);
	}
	*/
}


void CAddressUser::OnCloseWindow()
{
	((CAddrEditProperties*) GetParent())->CloseWindow();
}


BOOL CAddressUser::PerformOnOK(MSG_Pane *pane)
{
	// TODO: Add your specialized code here and/or call the base class

    // never visited this page so don't bother
	if (m_bActivated)
		UpdateData(TRUE);

	// remember to free old memory
	AB_AttributeValue *values = new AB_AttributeValue[NUM_ADDRESS_USER_ATTRIBUTES];

	values[0].attrib = AB_attribGivenName;
	values[0].u.string = XP_STRDUP(m_firstname.GetBuffer(0));

	values[1].attrib = AB_attribNickName;
	values[1].u.string = XP_STRDUP(m_nickname.GetBuffer(0));

	values[2].attrib = AB_attribFamilyName;
	values[2].u.string = XP_STRDUP(m_lastname.GetBuffer(0));

	values[3].attrib = AB_attribEmailAddress;
	values[3].u.string = XP_STRDUP(m_address.GetBuffer(0));

	values[4].attrib = AB_attribCompanyName;
	values[4].u.string = XP_STRDUP(m_company.GetBuffer(0));

	values[5].attrib = AB_attribTitle;
	values[5].u.string = XP_STRDUP(m_title.GetBuffer(0));

	values[6].attrib = AB_attribInfo;
	values[6].u.string = XP_STRDUP(m_description.GetBuffer(0));

	values[7].attrib = AB_attribHTMLMail;
	values[7].u.boolValue = m_useHTML;

	values[8].attrib = AB_attribDisplayName;
	values[8].u.string = XP_STRDUP(m_displayname.GetBuffer(0));

	AB_SetPersonEntryAttributes(pane, values, NUM_ADDRESS_USER_ATTRIBUTES);
	
	for(int i = 0; i < NUM_ADDRESS_USER_ATTRIBUTES; i++)
	{
		if(AB_IsStringEntryAttributeValue(&values[i]))
		{
			XP_FREE(values[i].u.string);
		}
	}

	delete values;
/*
	person->pNickName = m_nickname.GetBuffer(0);
	person->pGivenName = m_firstname.GetBuffer(0);
	person->pFamilyName = m_lastname.GetBuffer(0);
	person->pEmailAddress = m_address.GetBuffer(0);
	person->pInfo = m_description.GetBuffer(0);
	person->HTMLmail = m_useHTML;
	person->pCompanyName = m_company.GetBuffer(0);
	person->pTitle = m_title.GetBuffer(0);
*/
	return TRUE;
}

BOOL CAddressUser::OnKillActive( )
{
	CString formattedString;

	if(!CNetscapePropertyPage::OnKillActive())
        return(FALSE);

	formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), m_firstname + " " + m_lastname);
	GetParent()->SetWindowText(formattedString);
	return (TRUE);
}


BOOL CAddressUser::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}


/////////////////////////////////////////////////////////////////////////////
// CAddressContact property page

CAddressContact::CAddressContact(CWnd *pParent) 
	: CNetscapePropertyPage(CAddressContact::IDD)
{
	//{{AFX_DATA_INIT(CAddressContact)
	m_poaddress = _T("");
	m_country = _T("");
	m_address = _T("");
	m_locality = _T("");
	m_region = _T("");
	m_zip = _T(""); 
	m_work = _T(""); 
	m_fax = _T(""); 
	m_home = _T("");
	m_pager = _T("");
	m_cellular = _T("");
	//}}AFX_DATA_INIT
	m_bActivated = FALSE;
}

CAddressContact::~CAddressContact()
{
}

void CAddressContact::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddressContact)
	DDX_Text(pDX, IDC_STREET2, m_address);
	DDX_Text(pDX, IDC_LOCALITY, m_locality);
	DDX_Text(pDX, IDC_REGION, m_region);
	DDX_Text(pDX, IDC_COUNTRY, m_country);
//	DDX_Text(pDX, IDC_POBOX, m_poaddress);
	DDX_Text(pDX, IDC_ZIP, m_zip);
	DDX_Text(pDX, IDC_PHONE1, m_work);
	DDX_Text(pDX, IDC_FAX, m_fax);
	DDX_Text(pDX, IDC_HOME1, m_home);
	DDX_Text(pDX, IDC_PAGER, m_pager);
	DDX_Text(pDX, IDC_CELLULAR, m_cellular);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAddressContact, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CAddressContact)
		// NOTE: the ClassWizard will add message map macros here
		ON_BN_CLICKED(IDC_TO, OnCloseWindow)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddressContact message handlers

BOOL CAddressContact::OnInitDialog() 
{
	// TODO: Add your specialized code here and/or call the base class
	CNetscapePropertyPage::OnInitDialog();
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());
	return TRUE;
}

void CAddressContact::SetFonts(HFONT pFont)
{	
	::SendMessage(::GetDlgItem(m_hWnd, IDC_STREET2), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_LOCALITY), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_REGION), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_COUNTRY), WM_SETFONT, (WPARAM)pFont, FALSE);
//	::SendMessage(::GetDlgItem(m_hWnd, IDC_POBOX), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ZIP), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_PHONE1), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_FAX), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_HOME1), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_PAGER), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_CELLULAR), WM_SETFONT, (WPARAM)pFont, FALSE);

}


void CAddressContact::SetAttributes (MSG_Pane *pane)
{

	uint16 numAttributes = NUM_ADDRESS_CONTACT_ATTRIBUTES;

	AB_AttribID* attribs = new AB_AttribID[NUM_ADDRESS_CONTACT_ATTRIBUTES];

	attribs[0] = AB_attribPOAddress;
	attribs[1] = AB_attribStreetAddress;
	attribs[2] = AB_attribLocality;
	attribs[3] = AB_attribRegion;
	attribs[4] = AB_attribZipCode;
	attribs[5] = AB_attribCountry;
	attribs[6] = AB_attribWorkPhone;
	attribs[7] = AB_attribFaxPhone;
	attribs[8] = AB_attribHomePhone;
	attribs[9] = AB_attribPager;
	attribs[10] = AB_attribCellularPhone;

	AB_AttributeValue *values;

	AB_GetPersonEntryAttributes(pane, attribs, &values, &numAttributes);

	for(int i = 0; i < numAttributes; i++)
	{
		switch(values[i].attrib)
		{
			case AB_attribPOAddress:     m_poaddress = values[i].u.string;
									     break;
			case AB_attribStreetAddress: m_address = values[i].u.string;
									     break;
			case AB_attribLocality:		 m_locality = values[i].u.string;
									     break;
			case AB_attribRegion:		 m_region = values[i].u.string;
									     break;
			case AB_attribZipCode:		 m_zip = values[i].u.string;
										 break;
			case AB_attribCountry:		 m_country = values[i].u.string;
										 break;
			case AB_attribWorkPhone:	 m_work = values[i].u.string;
										 break;
			case AB_attribFaxPhone:		 m_fax = values[i].u.string;
										 break;
			case AB_attribHomePhone:     m_home = values[i].u.string;
										 break;
			case AB_attribPager:		 m_pager = values[i].u.string;
										 break;
			case AB_attribCellularPhone: m_cellular = values[i].u.string;
									     break;
		}



	}

	AB_FreeEntryAttributeValues(values, numAttributes);


/*	if (person) {
		if (person->pPOAddress)
			m_poaddress = person->pPOAddress;
		if (person->pAddress)
			m_address = person->pAddress;
		if (person->pLocality)
			m_locality = person->pLocality;
		if (person->pRegion)
			m_region = person->pRegion;
		if (person->pZipCode)
			m_zip = person->pZipCode;
		if (person->pCountry)
			m_country = person->pCountry;
		if (person->pWorkPhone)
			m_work = person->pWorkPhone;
		if (person->pFaxPhone)
			m_fax = person->pFaxPhone;
		if (person->pHomePhone)
			m_home = person->pHomePhone;
		return;
	}
	
	if (entryID)
	{
		// otherwise update the fields
		AB_GetPOAddress(m_dir, theApp.m_pABook, entryID, m_poaddress.GetBuffer(kMaxAddress));
		m_poaddress.ReleaseBuffer(-1);
		AB_GetStreetAddress(m_dir, theApp.m_pABook, entryID, m_address.GetBuffer(kMaxAddress));
		m_address.ReleaseBuffer(-1);
		AB_GetLocality(m_dir, theApp.m_pABook, entryID, m_locality.GetBuffer(kMaxLocalityLength));
		m_locality.ReleaseBuffer(-1);
		AB_GetRegion(m_dir, theApp.m_pABook, entryID, m_region.GetBuffer(kMaxRegionLength));
		m_region.ReleaseBuffer(-1);
		AB_GetZipCode(m_dir, theApp.m_pABook, entryID, m_zip.GetBuffer(kMaxZipCode));
		m_zip.ReleaseBuffer(-1);
		AB_GetCountry(m_dir, theApp.m_pABook, entryID, m_country.GetBuffer(kMaxAddress));
		m_country.ReleaseBuffer(-1);
		AB_GetWorkPhone(m_dir, theApp.m_pABook, entryID, m_work.GetBuffer(kMaxPhone));
		m_work.ReleaseBuffer(-1);
		AB_GetFaxPhone(m_dir, theApp.m_pABook, entryID, m_fax.GetBuffer(kMaxPhone));
		m_fax.ReleaseBuffer(-1);
		AB_GetHomePhone(m_dir, theApp.m_pABook, entryID, m_home.GetBuffer(kMaxPhone));
		m_home.ReleaseBuffer(-1);
	}
	*/

}


void CAddressContact::OnCloseWindow()
{
	((CAddrEditProperties*) GetParent())->CloseWindow();
}


BOOL CAddressContact::PerformOnOK(MSG_Pane *pane)
{
	// TODO: Add your specialized code here and/or call the base class

	if (m_bActivated)
		UpdateData(TRUE);

	AB_AttributeValue *values = new AB_AttributeValue[NUM_ADDRESS_CONTACT_ATTRIBUTES];

	values[0].attrib = AB_attribPOAddress;
	values[0].u.string = XP_STRDUP(m_poaddress.GetBuffer(0));

	values[1].attrib = AB_attribCountry;
	values[1].u.string = XP_STRDUP(m_country.GetBuffer(0));

	values[2].attrib = AB_attribStreetAddress;
	values[2].u.string = XP_STRDUP(m_address.GetBuffer(0));

	values[3].attrib = AB_attribLocality;
	values[3].u.string = XP_STRDUP(m_locality.GetBuffer(0));

	values[4].attrib = AB_attribRegion;
	values[4].u.string = XP_STRDUP(m_region.GetBuffer(0));

	values[5].attrib = AB_attribZipCode;
	values[5].u.string = XP_STRDUP(m_zip.GetBuffer(0));

	values[6].attrib = AB_attribWorkPhone;
	values[6].u.string = XP_STRDUP(m_work.GetBuffer(0));

	values[7].attrib = AB_attribFaxPhone;
	values[7].u.string = XP_STRDUP(m_fax.GetBuffer(0));

	values[8].attrib = AB_attribHomePhone;
	values[8].u.string = XP_STRDUP(m_home.GetBuffer(0));

	values[9].attrib = AB_attribPager;
	values[9].u.string = XP_STRDUP(m_pager.GetBuffer(0));

	values[10].attrib = AB_attribCellularPhone;
	values[10].u.string = XP_STRDUP(m_cellular.GetBuffer(0));


	AB_SetPersonEntryAttributes(pane, values, NUM_ADDRESS_CONTACT_ATTRIBUTES);
	
	for(int i = 0; i < NUM_ADDRESS_CONTACT_ATTRIBUTES; i++)
	{
		if(AB_IsStringEntryAttributeValue(&values[i]))
		{
			XP_FREE(values[i].u.string);
		}
	}

	delete values;

/*
	person->pPOAddress = m_poaddress.GetBuffer(0);
	person->pCountry = m_country.GetBuffer(0);
	person->pAddress = m_address.GetBuffer(0);
	person->pLocality = m_locality.GetBuffer(0);
	person->pRegion = m_region.GetBuffer(0);
	person->pZipCode = m_zip.GetBuffer(0);
	person->pWorkPhone = m_work.GetBuffer(0);
	person->pFaxPhone = m_fax.GetBuffer(0);
	person->pHomePhone = m_home.GetBuffer(0);
*/
	return TRUE;
}

BOOL CAddressContact::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);


    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// CAddressCooltalk property page

CAddressCooltalk::CAddressCooltalk(CWnd *pParent) 
	: CNetscapePropertyPage(CAddressCooltalk::IDD)
{
	//{{AFX_DATA_INIT(CAddressCooltalk)
	m_ipaddress = _T("");
	m_iUseServer = 0;
	//}}AFX_DATA_INIT
	m_bActivated = FALSE;
}

CAddressCooltalk::~CAddressCooltalk()
{
}

void CAddressCooltalk::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddressCooltalk)
	DDX_Text(pDX, IDC_IP_ADDRESS, m_ipaddress);
	DDX_CBIndex(pDX, IDC_CoolServer, m_iUseServer);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAddressCooltalk, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CAddressCooltalk)
	ON_BN_CLICKED(IDC_TO, OnCloseWindow)
	ON_CBN_SELENDOK(IDC_CoolServer, OnSelendokServer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CAddressCooltalk::SetFonts(HFONT pFont)
{	
	::SendMessage(::GetDlgItem(m_hWnd, IDC_IP_ADDRESS), WM_SETFONT, (WPARAM)pFont, FALSE);
}


/////////////////////////////////////////////////////////////////////////////
// CAddressCooltalk message handlers

void CAddressCooltalk::SetAttributes (MSG_Pane *pane)
{
	short useServer = 0;

	// if it is an entry that hasn't been added to the database yet then return
/*	if (person)
	{
		m_iUseServer = person->UseServer;

		if (m_iUseServer == 1) {
			m_specificDLS = person->pCoolAddress;
			m_ipaddress = m_specificDLS;
		}

		if (m_iUseServer == 2) {
			m_hostorIP = person->pCoolAddress;
			m_ipaddress = m_hostorIP;
		}
		return;
	}

	if (entryID)
	{
		AB_GetUseServer(m_dir, theApp.m_pABook, entryID, &useServer);
		m_iUseServer = useServer;

		if (m_iUseServer == 1) {
			AB_GetCoolAddress(m_dir, theApp.m_pABook, entryID, m_specificDLS.GetBuffer(kMaxCoolAddress));
			m_specificDLS.ReleaseBuffer(-1);
			m_ipaddress = m_specificDLS;
		}

		if (m_iUseServer == 2) {
			AB_GetCoolAddress(m_dir, theApp.m_pABook, entryID, m_hostorIP.GetBuffer(kMaxCoolAddress));
			m_hostorIP.ReleaseBuffer(-1);
			m_ipaddress = m_hostorIP;
		}
	}
	*/
}

void CAddressCooltalk::SetExplanationText()
{
	CString expl;
	switch (m_iUseServer)
	{
		case kDefaultDLS:
			expl = "";
			break;

		case kSpecificDLS:
			expl.LoadString(IDS_EXAMPLESPECIFICDLS);
			break;

		case kHostOrIPAddress:
			expl.LoadString(IDS_EXAMPLEHOSTNAME);
			break;
	}
	SetDlgItemText(IDC_EXPLANATION1, expl);
}

void CAddressCooltalk::OnSelendokServer()
{
	switch (m_iUseServer)
	{
		case kDefaultDLS:
			break;

		case kSpecificDLS:
            GetDlgItem(IDC_IP_ADDRESS)->GetWindowText(m_specificDLS);
			break;

		case kHostOrIPAddress:
            GetDlgItem(IDC_IP_ADDRESS)->GetWindowText(m_hostorIP);
			break;
	}

	UpdateData();
	switch (m_iUseServer)
	{
		case kDefaultDLS:
			GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(FALSE);
			SetDlgItemText(IDC_IP_ADDRESS, "");
			break;

		case kSpecificDLS:
			GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(TRUE);
			SetDlgItemText(IDC_IP_ADDRESS, m_specificDLS);
			break;

		case kHostOrIPAddress:
			GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(TRUE);
			SetDlgItemText(IDC_IP_ADDRESS, m_hostorIP );
			break;
	}
	SetExplanationText();
}

void CAddressCooltalk::OnCloseWindow()
{
	((CAddrEditProperties*) GetParent())->CloseWindow();
}

BOOL CAddressCooltalk::OnInitDialog() 
{
	// TODO: Add your specialized code here and/or call the base class
	CNetscapePropertyPage::OnInitDialog();
	UpdateData (FALSE);
	
	if (m_iUseServer == 0)
		GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(FALSE);
	SetExplanationText();
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());

	return TRUE;
}

BOOL CAddressCooltalk::PerformOnOK(MSG_Pane *pane) 
{
	// TODO: Add your specialized code here and/or call the base class
	if (m_bActivated)
		UpdateData();

/*	person->pCoolAddress = m_ipaddress.GetBuffer(0);
	person->UseServer = m_iUseServer;
*/
	return TRUE;
}

BOOL CAddressCooltalk::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());
	m_bActivated = TRUE;
	return(TRUE);
}


////////////////////////////////////////////////////////////////////////////
// CAddrLDAPProperties

CAddrLDAPProperties::CAddrLDAPProperties (CWnd * parent,
		MWContext* context,
		DIR_Server*  dir,
		LPCTSTR lpszCaption) :
    CNetscapePropertySheet ( lpszCaption, parent )
{

		// for New server only
	DIR_InitServer(&m_serverInfo);
	m_serverInfo.dirType = LDAPDirectory;
	m_serverInfo.saveResults = TRUE;
	m_pLDAPProperties = NULL;
	m_pOfflineProperties = NULL;
	m_pExistServer = dir;
	m_context = context;
}

CAddrLDAPProperties::~CAddrLDAPProperties ( )
{
	if (m_pFont) {
		theApp.ReleaseAppFont(m_pFont);
	}
	if ( m_pLDAPProperties )
        delete m_pLDAPProperties;
	if ( m_pOfflineProperties )
        delete m_pOfflineProperties;
}

void CAddrLDAPProperties::OnHelp()
{
	if (GetActivePage() == m_pLDAPProperties)
		NetHelp(HELP_EDIT_USER);
	else if (GetActivePage() == m_pOfflineProperties)
		NetHelp(HELP_EDIT_USER_CONTACT);
}

BEGIN_MESSAGE_MAP(CAddrLDAPProperties, CNetscapePropertySheet)
	//{{AFX_MSG_MAP(CAddrLDAPProperties)
		// NOTE: the ClassWizard will add message map macros here
	    ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


int CAddrLDAPProperties::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	m_MailNewsResourceSwitcher.Reset();
	if (CNetscapePropertySheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

int CAddrLDAPProperties::DoModal()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;

	m_pLDAPProperties = new CServerDialog (this, m_pExistServer, &m_serverInfo);
	m_pOfflineProperties = new CServerOfflineDialog (this, m_pExistServer, &m_serverInfo);
	AddPage( m_pLDAPProperties );
	AddPage( m_pOfflineProperties );

	return CNetscapePropertySheet::DoModal();
}


BOOL CAddrLDAPProperties::OnInitDialog ( )
{            
	BOOL bResult = TRUE;
	CString label;   

	m_MailNewsResourceSwitcher.Reset();
	int16 guicsid = 0;
	if (m_context)
	{
		INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_context);
		guicsid = INTL_GetCSIWinCSID(csi);
	}	
	else
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
#ifdef _WIN32		
	bResult = CNetscapePropertySheet::OnInitDialog();
#endif

	if (m_pExistServer)
	{
		label.LoadString(IDS_LDAP_SERVER_PROPERTY);
		SetWindowText(LPCTSTR(label));
	}

	return bResult;
}


/////////////////////////////////////////////////////////////////////////////
// CServerDialog		   
CServerDialog::CServerDialog(CWnd *pParent, DIR_Server *pExistServer, 
							 DIR_Server *pNewServer)
	: CNetscapePropertyPage(CServerDialog::IDD)
{
	m_pExistServer = pExistServer;

	// for New server only
	m_serverInfo =pNewServer;
	m_bActivated = FALSE;
}

BOOL CServerDialog::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();

	if (m_pExistServer)
	{
		SetDlgItemText(IDC_EDIT_DESCRIPTION, m_pExistServer->description);
		if (m_pExistServer->dirType != PABDirectory)
		{
			SetDlgItemText(IDC_EDIT_SERVER, m_pExistServer->serverName);
			SetDlgItemText(IDC_EDIT_ROOT, m_pExistServer->searchBase);
			SetDlgItemInt(IDC_EDIT_PORT_NO, m_pExistServer->port);
			SetDlgItemInt(IDC_EDIT_MAX_HITS, m_pExistServer->maxHits);
			HG19511
			CheckDlgButton(IDC_SAVE_PASSWORD, m_pExistServer->savePassword);
			CheckDlgButton(IDC_LOGIN_LDAP, m_pExistServer->enableAuth);
		}
		else
		{
			GetDlgItem(IDC_EDIT_SERVER)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_ROOT)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_PORT_NO)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_MAX_HITS)->EnableWindow(FALSE);
			HG17271
			GetDlgItem(IDC_SAVE_PASSWORD)->EnableWindow(FALSE);
			GetDlgItem(IDC_LOGIN_LDAP)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_SERVER)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_ROOT)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_PORT)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_MAX_HITS)->EnableWindow(FALSE);
		}
	}
	else
	{
		SetDlgItemInt(IDC_EDIT_MAX_HITS, 100);
		SetDlgItemInt(IDC_EDIT_PORT_NO, LDAP_PORT);
	}
#ifdef _WIN32
	((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->SetLimitText(MAX_DESCRIPTION_LEN - 1);
	((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->SetLimitText(MAX_HOSTNAME_LEN - 1);
#else
	((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->LimitText(MAX_DESCRIPTION_LEN - 1);
	((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->LimitText(MAX_HOSTNAME_LEN - 1);
	// we aren't doing secure ldap on win16 so hide the checkbox
	HG18671
#endif
	((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->SetFocus();
	OnEnableLoginLDAP();
	return 0;
}

BOOL CServerDialog::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}

BOOL CServerDialog::OnKillActive( )
{
	if (!ValidDataInput())
		return FALSE;
	return TRUE;
}

void CServerDialog::OnOK() 
{
    CNetscapePropertyPage::OnOK();

    char text[MAX_DESCRIPTION_LEN];
	if (m_pExistServer)
	{
		if (GetDlgItemText(IDC_EDIT_DESCRIPTION, text, MAX_DESCRIPTION_LEN))
		{
			XP_FREE(m_pExistServer->description);
			m_pExistServer->description = XP_STRDUP(text);
		}
		if (m_pExistServer->dirType == PABDirectory)
			return;
		if (GetDlgItemText(IDC_EDIT_SERVER, text, MAX_DESCRIPTION_LEN))
		{
			XP_FREE(m_pExistServer->serverName);
			m_pExistServer->serverName = XP_STRDUP(text);
		}
		if (GetDlgItemText(IDC_EDIT_ROOT, text, MAX_DESCRIPTION_LEN))
		{
			XP_FREE(m_pExistServer->searchBase);
			m_pExistServer->searchBase = XP_STRDUP(text);
		}
		m_pExistServer->port = (int)GetDlgItemInt(IDC_EDIT_PORT_NO);
		m_pExistServer->maxHits = (int)GetDlgItemInt(IDC_EDIT_MAX_HITS);
		HG19616
		if (IsDlgButtonChecked(IDC_SAVE_PASSWORD))
			m_pExistServer->savePassword = TRUE;
		else
			m_pExistServer->savePassword = FALSE;
		if (IsDlgButtonChecked(IDC_LOGIN_LDAP))
			m_pExistServer->enableAuth = TRUE;
		else
			m_pExistServer->enableAuth = FALSE;
	}
	else
	{
		char port[16];
		if (GetDlgItemText(IDC_EDIT_DESCRIPTION, text, MAX_DESCRIPTION_LEN))
			m_serverInfo->description = XP_STRDUP(text);
		if (GetDlgItemText(IDC_EDIT_SERVER, text, MAX_DESCRIPTION_LEN))
			m_serverInfo->serverName = XP_STRDUP(text);
		if (GetDlgItemText(IDC_EDIT_ROOT, text, MAX_DESCRIPTION_LEN))
			m_serverInfo->searchBase = XP_STRDUP(text);
		HG19879
		if (GetDlgItemText(IDC_EDIT_PORT_NO, port, 16) > 0)
			m_serverInfo->port = atoi(port);
		else
		{
			HG17922
				m_serverInfo->port = LDAP_PORT;
		}
		m_serverInfo->maxHits = (int)GetDlgItemInt(IDC_EDIT_MAX_HITS);
		if (IsDlgButtonChecked(IDC_SAVE_PASSWORD))
			m_serverInfo->savePassword = TRUE;
		else
			m_serverInfo->savePassword = FALSE;
		if (IsDlgButtonChecked(IDC_LOGIN_LDAP))
			m_serverInfo->enableAuth = TRUE;
		else
			m_serverInfo->enableAuth = FALSE;
	}
}

void CServerDialog::OnCheckSecure() 
{
	HG98219
}

void CServerDialog::OnEnableLoginLDAP() 
{
	if (IsDlgButtonChecked(IDC_LOGIN_LDAP))
		GetDlgItem(IDC_SAVE_PASSWORD)->EnableWindow(TRUE);
	else {
		GetDlgItem(IDC_SAVE_PASSWORD)->EnableWindow(FALSE);
		CButton* button = (CButton*) GetDlgItem(IDC_SAVE_PASSWORD);
		button->SetCheck(FALSE);
	}
}

BOOL CServerDialog::ValidDataInput()
{
    char text[MAX_DESCRIPTION_LEN];
	if (0 == GetDlgItemText(IDC_EDIT_DESCRIPTION, text, MAX_DESCRIPTION_LEN))
	{
		CAddrFrame::HandleErrorReturn(0, this, IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->SetFocus();
		return FALSE;
	}

	if (m_pExistServer && m_pExistServer->dirType == PABDirectory)
		return TRUE;

	if (0 == GetDlgItemText(IDC_EDIT_SERVER, text, MAX_DESCRIPTION_LEN))
	{
		CAddrFrame::HandleErrorReturn(0, this, IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->SetFocus();
		return FALSE;
	}
	if (GetDlgItemText(IDC_EDIT_PORT_NO, text, MAX_DESCRIPTION_LEN))
	{
		int nPort = GetDlgItemInt(IDC_EDIT_PORT_NO);
		if (nPort < 0 && nPort> MAX_PORT_NUMBER)
		{
			CAddrFrame::HandleErrorReturn(0, this, IDS_PORT_RANGE);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;	    
		}
		if (!::IsNumeric(text))
		{
			CAddrFrame::HandleErrorReturn(0, this, IDS_NUMBERS_ONLY);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;	    
		}
	}
	if (GetDlgItemText(IDC_EDIT_MAX_HITS, text, MAX_DESCRIPTION_LEN))
	{
		if (!::IsNumeric(text))
		{
			CAddrFrame::HandleErrorReturn(0, this, IDS_NUMBERS_ONLY);
			((CEdit*)GetDlgItem(IDC_EDIT_MAX_HITS))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_MAX_HITS))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;
		}
	}
	return TRUE;
}

void CServerDialog::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
}

void CServerDialog::OnHelp() 
{
	NetHelp(HELP_LDAP_SERVER_PROPS);
}

BEGIN_MESSAGE_MAP(CServerDialog, CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_SECURE, OnCheckSecure)
	ON_BN_CLICKED(IDC_LOGIN_LDAP, OnEnableLoginLDAP)
	ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CServerOfflineDialog		   
CServerOfflineDialog::CServerOfflineDialog(CWnd *pParent, DIR_Server *pExistServer, 
							 DIR_Server *pNewServer)
	:  CNetscapePropertyPage(CServerOfflineDialog::IDD)
{
	m_pExistServer = pExistServer;

	// for New server only
	m_serverInfo = pNewServer;
	m_bActivated = FALSE;
}

BOOL CServerOfflineDialog::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();

	if (m_pExistServer)
	{
		CheckDlgButton(IDC_CHECK1, DIR_TestFlag  (m_pExistServer, DIR_REPLICATION_ENABLED));
	}
	else
	{
		CheckDlgButton(IDC_CHECK1, FALSE);
	}

	return 0;
}

BOOL CServerOfflineDialog::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}

void CServerOfflineDialog::OnOK() 
{
    CNetscapePropertyPage::OnOK();

	if (m_bActivated)
	{
		if (m_pExistServer)
		{
			if (IsDlgButtonChecked(IDC_CHECK1))
				DIR_SetFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
			else
				DIR_ClearFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
		}
		else
		{
			if (IsDlgButtonChecked(IDC_CHECK1))
				DIR_SetFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
			else
				DIR_ClearFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
		}
	}
}

void CServerOfflineDialog::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
}

void CServerOfflineDialog::OnHelp() 
{
	NetHelp(HELP_LDAP_SERVER_PROPS);
}

void CServerOfflineDialog::OnUpdateNow() 
{
	BOOL bDownload = FALSE;

	DIR_Server *pServer = (m_pExistServer) ? m_pExistServer : m_serverInfo;

	if(pServer)
	{
		DIR_SetFlag (pServer, DIR_REPLICATION_ENABLED);
		NET_ReplicateDirectory(NULL, pServer);
	}

	GetParent()->PostMessage(WM_COMMAND, IDOK, 0);
	
}

BEGIN_MESSAGE_MAP(CServerOfflineDialog, CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_UPDATE_NOW, OnUpdateNow)
	ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()



#else  // MOZ_NEWADDR

// addrprop.cpp : implementation file for property dialogs from
// the address book
//

#include "addrfrm.h"
#include "template.h"
#include "xpgetstr.h"
#include "wfemsg.h"
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

extern "C" {
#include "xpgetstr.h"
extern int MK_ADDR_BOOK_CARD;

BOOL IsNumeric(char* pStr);
};

////////////////////////////////////////////////////////////////////////////
// CAddrEditProperities

CAddrEditProperties::CAddrEditProperties (CAddrFrame* frameref,
											DIR_Server*  dir,
											LPCTSTR lpszCaption, CWnd * parent, 
											ABID entryID,
											PersonEntry* person,
											MWContext* context) :
    CNetscapePropertySheet ( lpszCaption, parent )
{

	m_pUserProperties = NULL;
	m_pContact = NULL;
	m_pSecurity = NULL;
	m_pCooltalk = NULL;
	m_entryID = entryID;
	m_dir = dir;
	m_context = context;
	m_pPerson = person;
	m_frame = frameref;
}

CAddrEditProperties::~CAddrEditProperties ( )
{
	if (m_pFont) {
		theApp.ReleaseAppFont(m_pFont);
	}
	if ( m_pUserProperties )
        delete m_pUserProperties;
	if ( m_pContact )
        delete m_pContact;
	if ( m_pSecurity )
        delete m_pSecurity;
	if ( m_pCooltalk )
        delete m_pCooltalk;
}

BOOL CAddrEditProperties::Create(CWnd* pParentWnd, DWORD dwStyle, DWORD dwExStyle)
{
	BOOL ret = FALSE;
	if (m_MailNewsResourceSwitcher.Initialize ()) {
		m_pUserProperties = new CAddressUser (this);
		m_pContact = new CAddressContact (this);
		m_pSecurity = NULL;
		m_pCooltalk = new CAddressCooltalk (this);
		AddPage( m_pUserProperties );
		AddPage( m_pContact );
		AddPage( m_pCooltalk );
		SetEntryID (m_dir, m_entryID, m_pPerson);
		CNetscapePropertySheet::Create (pParentWnd, dwStyle, dwExStyle);
		ret = TRUE;
	}
	return ret;
}

int CAddrEditProperties::DoModal()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;
	m_pUserProperties = new CAddressUser (this);
	m_pContact = new CAddressContact (this);
	m_pSecurity = NULL;
	m_pCooltalk = new CAddressCooltalk (this);
	AddPage( m_pUserProperties );
	AddPage( m_pContact );
	AddPage( m_pCooltalk );
	SetEntryID (m_dir, m_entryID, m_pPerson);
	return CNetscapePropertySheet::DoModal();
}


void CAddrEditProperties::OnHelp()
{
	if (m_entryID == 0)
	{
		if (GetActivePage() == m_pUserProperties)
			NetHelp(HELP_ADD_USER_PROPS);
		else if (GetActivePage() == m_pContact)
			NetHelp(HELP_ADD_USER_CONTACT);
		HG92710
		else if (GetActivePage() == m_pCooltalk)
			NetHelp(HELP_ADD_USER_NETSCAPE_COOLTALK);
	}
	else
	{
		if (GetActivePage() == m_pUserProperties)
			NetHelp(HELP_EDIT_USER);
		else if (GetActivePage() == m_pContact)
			NetHelp(HELP_EDIT_USER_CONTACT);
		HG27626
		else if (GetActivePage() == m_pCooltalk)
			NetHelp(HELP_EDIT_USER_CALLPOINT);
	}
}

BEGIN_MESSAGE_MAP(CAddrEditProperties, CNetscapePropertySheet)
	//{{AFX_MSG_MAP(CAddrEditProperties)
		// NOTE: the ClassWizard will add message map macros here
		ON_BN_CLICKED(IDC_TO, CloseWindow)
	    ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CAddrEditProperties::SetEntryID (DIR_Server* dir, ABID entryID, PersonEntry* person)
{
	// otherwise just switch the contents of the sheet
	((CAddressUser*)m_pUserProperties)->SetEntryID(m_dir, entryID, person);
	((CAddressContact*)m_pContact)->SetEntryID(m_dir, entryID, person);
	((CAddressCooltalk*)m_pCooltalk)->SetEntryID(m_dir, entryID, person);
}

int CAddrEditProperties::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	m_MailNewsResourceSwitcher.Reset();
	if (CNetscapePropertySheet::OnCreate(lpCreateStruct) == -1)
		return -1;
#ifdef XP_WIN16
	OnInitDialog();
#endif
	return 0;
}

static int rgiButtons[] = { IDOK, IDC_TO, IDCANCEL, ID_APPLY_NOW, IDHELP };



BOOL CAddrEditProperties::OnInitDialog ( )
{            
	BOOL bResult = TRUE;
	CString label;   

	m_MailNewsResourceSwitcher.Reset();
	int16 guicsid = 0;
	if (m_context)
	{
		INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_context);
		guicsid = INTL_GetCSIWinCSID(csi);
	}	
	else
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
		
	bResult = CNetscapePropertySheet::OnInitDialog();

	if (m_bModeless)
	{
		// layout property sheet so button area IS accounted for
		CRect rectWnd;
		GetWindowRect(rectWnd);
		CRect rectButton;
		HWND hWnd = ::GetDlgItem(m_hWnd, IDOK);
		ASSERT(hWnd != NULL);
		::GetWindowRect(hWnd, rectButton);

		SetWindowPos(NULL, 0, 0,
			rectWnd.Width(), rectButton.bottom - rectWnd.top + 8,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

		// We don't use apply
		CWnd* hWnd2 = GetDlgItem(ID_APPLY_NOW);
		if (hWnd2)
			hWnd2->DestroyWindow ();

		CWnd* widget;
		CRect rect2, rect3;

		// create the CLOSE button
		widget = GetDlgItem(IDCANCEL);
		widget->GetWindowRect(&rect2);
		widget->GetClientRect(&rect3);
		ScreenToClient(&rect2);

		widget->DestroyWindow ();

		CButton * pButton = new CButton;
		label.LoadString (IDS_CANCEL_BUTTON);
		pButton->Create( label, 
			WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			rect3, this, IDC_TO);
		pButton->MoveWindow(&rect2, TRUE);
		pButton->SetFont(GetDlgItem(IDOK)->GetFont(), TRUE);

		// readd some of the standard buttons for modeless dialogs
		for (int i = 0; i < sizeof(rgiButtons)/sizeof(rgiButtons[0]); i++)
		{
			HWND hWnd = ::GetDlgItem(m_hWnd, rgiButtons[i]);
			if (hWnd != NULL)
			{
				::ShowWindow(hWnd, SW_SHOW);
				::EnableWindow(hWnd, TRUE);
			}
		}
	}


	return bResult;
}

void CAddrEditProperties::CloseWindow()
{
	CButton * pButton = NULL;
	pButton = (CButton *) GetDlgItem (IDC_TO);
	if (pButton)
		delete pButton;

#ifdef XP_WIN16
	if (m_bModeless)
	{
		pButton = (CButton *) GetDlgItem (IDHELP);
		if (pButton)
			delete pButton;
		pButton = (CButton *) GetDlgItem (IDOK);
		if (pButton)
			delete pButton;
	}
#endif
#ifdef XP_WIN32
	if (m_bModeless)
		OnClose();  
	else
		EndDialog(IDOK);
#else
	if (m_bModeless)
		DestroyWindow();
	else
		EndDialog (IDOK);
#endif
}

/****************************************************************************
*
*	CAddrEditProperties::OnOK
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We override this function because we are a modeless window.
*
****************************************************************************/

void CAddrEditProperties::OnOK()
{
	PersonEntry		person;
	int				errorID = 0;

	// add or modify a user
	person.Initialize();

	((CAddressUser*)m_pUserProperties)->PerformOnOK(&person);
	((CAddressContact*)m_pContact)->PerformOnOK(&person);
	((CAddressCooltalk*)m_pCooltalk)->PerformOnOK(&person);

	// if this is a new user then we will need to create an entry
	if (GetEntryID() == NULL) {
		ABID			entryID;
		if ((errorID = AB_AddUser (m_dir, theApp.m_pABook, &person, &entryID)) != 0) {
			CString s;
			CAddrFrame::HandleErrorReturn(errorID, GetParent());
			SetCurrentPage(0);
			m_pUserProperties->GetDlgItem(IDC_FIRSTNAME)->SetFocus();
			return;
		}
	}
	else {
		// we are potentially modifying a user so we will edit them
		if ((errorID = AB_ModifyUser(m_dir, theApp.m_pABook, GetEntryID(), &person)) != 0) {
			CString s;
			CAddrFrame::HandleErrorReturn(errorID, GetParent());
			SetCurrentPage(0);
			m_pUserProperties->GetDlgItem(IDC_FIRSTNAME)->SetFocus();
			return;
		}
	}
	
	CButton * pButton = NULL;
	pButton = (CButton *) GetDlgItem (IDC_TO);
	if (pButton)
		delete pButton;

#ifdef XP_WIN16
	if (m_bModeless)
	{
		pButton = (CButton *) GetDlgItem (IDHELP);
		if (pButton)
			delete pButton;
		pButton = (CButton *) GetDlgItem (IDOK);
		if (pButton)
			delete pButton;
	}
#endif

	if (m_bModeless)
		DestroyWindow();
	else
		EndDialog (IDOK);

} // END OF	FUNCTION CAddrEditProperities::OnOK()


/////////////////////////////////////////////////////////////////////////////
// CAddressUser property page

CAddressUser::CAddressUser(CWnd *pParent) 
	: CNetscapePropertyPage(CAddressUser::IDD)
{
	//{{AFX_DATA_INIT(CAddressUser)
	m_address = _T("");
	m_description = _T("");
	m_firstname = _T("");
	m_lastname = _T("");
	m_nickname = _T("");
	m_useHTML = 0;
	m_company = _T("");
	m_title = _T("");
	//}}AFX_DATA_INIT
	m_bActivated = FALSE;
}

CAddressUser::~CAddressUser()
{
}

void CAddressUser::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddressUser)
	DDX_Text(pDX, IDC_COMPANY_NAME, m_company);
	DDX_Text(pDX, IDC_TITLE, m_title);
	DDX_Text(pDX, IDC_ADDRESS, m_address);
	DDX_Text(pDX, IDC_Description, m_description);
	DDX_Text(pDX, IDC_FIRSTNAME, m_firstname);
	DDX_Text(pDX, IDC_LASTNAME, m_lastname);
	DDX_Text(pDX, IDC_NICKNAME, m_nickname);
	DDX_Check(pDX, IDC_CHECK1, m_useHTML);
	DDX_Text(pDX, IDC_DEPARTMENT, m_department);
	DDX_Text(pDX, IDC_DISPLAYNAME, m_displayname);
	//}}AFX_DATA_MAP

}

void CAddressUser::SetFonts(HFONT pFont)
{	
	::SendMessage(::GetDlgItem(m_hWnd, IDC_COMPANY_NAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_TITLE), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ADDRESS), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_Description), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_FIRSTNAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_LASTNAME), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_NICKNAME), WM_SETFONT, (WPARAM)pFont, FALSE);
}


BEGIN_MESSAGE_MAP(CAddressUser, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CAddressUser)
		// NOTE: the ClassWizard will add message map macros here
		ON_BN_CLICKED(IDC_TO, OnCloseWindow)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddressUser message handlers

BOOL CAddressUser::OnInitDialog() 
{
	// TODO: Add your specialized code here and/or call the base class
	CNetscapePropertyPage::OnInitDialog();
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());
	return TRUE;
}

void CAddressUser::SetEntryID (DIR_Server* dir, ABID entryID, PersonEntry* person)
{
	m_dir = dir;

	// if it is an entry that hasn't been added to the database yet then return
	if (person) {
		if (person->pGivenName)
			m_firstname = person->pGivenName;
		if (person->pNickName)
			m_nickname = person->pNickName;
		if (person->pFamilyName)
			m_lastname = person->pFamilyName;
		if (person->pEmailAddress)
			m_address = person->pEmailAddress;
		if (person->pCompanyName)
			m_company = person->pCompanyName;
		if (person->pTitle)
			m_title = person->pTitle;
		if (person->pInfo)
			m_description = person->pInfo;
		if (person->HTMLmail)
			m_useHTML = person->HTMLmail;
		return;
	}

	if (entryID) {
		// otherwise update the fields
		AB_GetGivenName(m_dir, theApp.m_pABook, entryID, m_firstname.GetBuffer(kMaxNameLength));
		m_firstname.ReleaseBuffer(-1);
		AB_GetNickname(m_dir, theApp.m_pABook, entryID, m_nickname.GetBuffer(kMaxNameLength));
		m_nickname.ReleaseBuffer(-1);
		AB_GetFamilyName(m_dir, theApp.m_pABook, entryID, m_lastname.GetBuffer(kMaxNameLength));
		m_lastname.ReleaseBuffer(-1);
		AB_GetEmailAddress(m_dir, theApp.m_pABook, entryID, m_address.GetBuffer(kMaxEmailAddressLength));
		m_address.ReleaseBuffer(-1);
		AB_GetInfo(m_dir, theApp.m_pABook, entryID, m_description.GetBuffer(kMaxInfo));
		m_description.ReleaseBuffer(-1);
		XP_Bool useHTML = FALSE;
		AB_GetHTMLMail(m_dir, theApp.m_pABook, entryID, &useHTML);
		m_useHTML = useHTML;
		AB_GetCompanyName(m_dir, theApp.m_pABook, entryID, m_company.GetBuffer(kMaxCompanyLength));
		m_company.ReleaseBuffer(-1);
		AB_GetTitle(m_dir, theApp.m_pABook, entryID, m_title.GetBuffer(kMaxTitle));
		m_title.ReleaseBuffer(-1);
	}
}


void CAddressUser::OnCloseWindow()
{
	((CAddrEditProperties*) GetParent())->CloseWindow();
}


BOOL CAddressUser::PerformOnOK(PersonEntry* person)
{
	// TODO: Add your specialized code here and/or call the base class

    // never visited this page so don't bother
	if (m_bActivated)
		UpdateData(TRUE);

	person->pNickName = m_nickname.GetBuffer(0);
	person->pGivenName = m_firstname.GetBuffer(0);
	person->pFamilyName = m_lastname.GetBuffer(0);
	person->pEmailAddress = m_address.GetBuffer(0);
	person->pInfo = m_description.GetBuffer(0);
	person->HTMLmail = m_useHTML;
	person->pCompanyName = m_company.GetBuffer(0);
	person->pTitle = m_title.GetBuffer(0);

	return TRUE;
}

BOOL CAddressUser::OnKillActive( )
{
	CString formattedString;

	if(!CNetscapePropertyPage::OnKillActive())
        return(FALSE);

	formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), m_firstname + " " + m_lastname);
	GetParent()->SetWindowText(formattedString);
	return (TRUE);
}


BOOL CAddressUser::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}


/////////////////////////////////////////////////////////////////////////////
// CAddressContact property page

CAddressContact::CAddressContact(CWnd *pParent) 
	: CNetscapePropertyPage(CAddressContact::IDD)
{
	//{{AFX_DATA_INIT(CAddressContact)
	m_poaddress = _T("");
	m_country = _T("");
	m_address = _T("");
	m_locality = _T("");
	m_region = _T("");
	m_zip = _T(""); 
	m_work = _T(""); 
	m_fax = _T(""); 
	m_home = _T(""); 
	//}}AFX_DATA_INIT
	m_bActivated = FALSE;
}

CAddressContact::~CAddressContact()
{
}

void CAddressContact::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddressContact)
	DDX_Text(pDX, IDC_STREET2, m_address);
	DDX_Text(pDX, IDC_LOCALITY, m_locality);
	DDX_Text(pDX, IDC_REGION, m_region);
	DDX_Text(pDX, IDC_COUNTRY, m_country);
	//DDX_Text(pDX, IDC_POBOX, m_poaddress);
	DDX_Text(pDX, IDC_ZIP, m_zip);
	DDX_Text(pDX, IDC_PHONE1, m_work);
	DDX_Text(pDX, IDC_FAX, m_fax);
	DDX_Text(pDX, IDC_HOME1, m_home);
	DDX_Text(pDX, IDC_PAGER, m_pager);
	DDX_Text(pDX, IDC_CELLULAR, m_cellular);
	//}}AFX_DATA_MAP

}


BEGIN_MESSAGE_MAP(CAddressContact, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CAddressContact)
		// NOTE: the ClassWizard will add message map macros here
		ON_BN_CLICKED(IDC_TO, OnCloseWindow)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddressContact message handlers

BOOL CAddressContact::OnInitDialog() 
{
	// TODO: Add your specialized code here and/or call the base class
	CNetscapePropertyPage::OnInitDialog();
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());
	return TRUE;
}

void CAddressContact::SetFonts(HFONT pFont)
{	
	::SendMessage(::GetDlgItem(m_hWnd, IDC_STREET2), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_LOCALITY), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_REGION), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_COUNTRY), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_POBOX), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_ZIP), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_PHONE1), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_FAX), WM_SETFONT, (WPARAM)pFont, FALSE);
	::SendMessage(::GetDlgItem(m_hWnd, IDC_HOME1), WM_SETFONT, (WPARAM)pFont, FALSE);
}


void CAddressContact::SetEntryID (DIR_Server* dir, ABID entryID, PersonEntry* person)
{
	m_dir = dir;

	if (person) {
		if (person->pPOAddress)
			m_poaddress = person->pPOAddress;
		if (person->pAddress)
			m_address = person->pAddress;
		if (person->pLocality)
			m_locality = person->pLocality;
		if (person->pRegion)
			m_region = person->pRegion;
		if (person->pZipCode)
			m_zip = person->pZipCode;
		if (person->pCountry)
			m_country = person->pCountry;
		if (person->pWorkPhone)
			m_work = person->pWorkPhone;
		if (person->pFaxPhone)
			m_fax = person->pFaxPhone;
		if (person->pHomePhone)
			m_home = person->pHomePhone;
		return;
	}
	
	if (entryID)
	{
		// otherwise update the fields
		AB_GetPOAddress(m_dir, theApp.m_pABook, entryID, m_poaddress.GetBuffer(kMaxAddress));
		m_poaddress.ReleaseBuffer(-1);
		AB_GetStreetAddress(m_dir, theApp.m_pABook, entryID, m_address.GetBuffer(kMaxAddress));
		m_address.ReleaseBuffer(-1);
		AB_GetLocality(m_dir, theApp.m_pABook, entryID, m_locality.GetBuffer(kMaxLocalityLength));
		m_locality.ReleaseBuffer(-1);
		AB_GetRegion(m_dir, theApp.m_pABook, entryID, m_region.GetBuffer(kMaxRegionLength));
		m_region.ReleaseBuffer(-1);
		AB_GetZipCode(m_dir, theApp.m_pABook, entryID, m_zip.GetBuffer(kMaxZipCode));
		m_zip.ReleaseBuffer(-1);
		AB_GetCountry(m_dir, theApp.m_pABook, entryID, m_country.GetBuffer(kMaxAddress));
		m_country.ReleaseBuffer(-1);
		AB_GetWorkPhone(m_dir, theApp.m_pABook, entryID, m_work.GetBuffer(kMaxPhone));
		m_work.ReleaseBuffer(-1);
		AB_GetFaxPhone(m_dir, theApp.m_pABook, entryID, m_fax.GetBuffer(kMaxPhone));
		m_fax.ReleaseBuffer(-1);
		AB_GetHomePhone(m_dir, theApp.m_pABook, entryID, m_home.GetBuffer(kMaxPhone));
		m_home.ReleaseBuffer(-1);
	}
}


void CAddressContact::OnCloseWindow()
{
	((CAddrEditProperties*) GetParent())->CloseWindow();
}


BOOL CAddressContact::PerformOnOK(PersonEntry* person)
{
	// TODO: Add your specialized code here and/or call the base class

	if (m_bActivated)
		UpdateData(TRUE);

	person->pPOAddress = m_poaddress.GetBuffer(0);
	person->pCountry = m_country.GetBuffer(0);
	person->pAddress = m_address.GetBuffer(0);
	person->pLocality = m_locality.GetBuffer(0);
	person->pRegion = m_region.GetBuffer(0);
	person->pZipCode = m_zip.GetBuffer(0);
	person->pWorkPhone = m_work.GetBuffer(0);
	person->pFaxPhone = m_fax.GetBuffer(0);
	person->pHomePhone = m_home.GetBuffer(0);

	return TRUE;
}

BOOL CAddressContact::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);


    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// CAddressCooltalk property page

CAddressCooltalk::CAddressCooltalk(CWnd *pParent) 
	: CNetscapePropertyPage(CAddressCooltalk::IDD)
{
	//{{AFX_DATA_INIT(CAddressCooltalk)
	m_ipaddress = _T("");
	m_iUseServer = 0;
	//}}AFX_DATA_INIT
	m_bActivated = FALSE;
}

CAddressCooltalk::~CAddressCooltalk()
{
}

void CAddressCooltalk::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddressCooltalk)
	DDX_Text(pDX, IDC_IP_ADDRESS, m_ipaddress);
	DDX_CBIndex(pDX, IDC_CoolServer, m_iUseServer);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAddressCooltalk, CNetscapePropertyPage)
	//{{AFX_MSG_MAP(CAddressCooltalk)
	ON_BN_CLICKED(IDC_TO, OnCloseWindow)
	ON_CBN_SELENDOK(IDC_CoolServer, OnSelendokServer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CAddressCooltalk::SetFonts(HFONT pFont)
{	
	::SendMessage(::GetDlgItem(m_hWnd, IDC_IP_ADDRESS), WM_SETFONT, (WPARAM)pFont, FALSE);
}


/////////////////////////////////////////////////////////////////////////////
// CAddressCooltalk message handlers

void CAddressCooltalk::SetEntryID(DIR_Server* dir, ABID entryID, PersonEntry* person) 
{
	m_dir = dir;
	short useServer = 0;

	// if it is an entry that hasn't been added to the database yet then return
	if (person)
	{
		m_iUseServer = person->UseServer;

		if (m_iUseServer == 1) {
			m_specificDLS = person->pCoolAddress;
			m_ipaddress = m_specificDLS;
		}

		if (m_iUseServer == 2) {
			m_hostorIP = person->pCoolAddress;
			m_ipaddress = m_hostorIP;
		}
		return;
	}

	if (entryID)
	{
		AB_GetUseServer(m_dir, theApp.m_pABook, entryID, &useServer);
		m_iUseServer = useServer;

		if (m_iUseServer == 1) {
			AB_GetCoolAddress(m_dir, theApp.m_pABook, entryID, m_specificDLS.GetBuffer(kMaxCoolAddress));
			m_specificDLS.ReleaseBuffer(-1);
			m_ipaddress = m_specificDLS;
		}

		if (m_iUseServer == 2) {
			AB_GetCoolAddress(m_dir, theApp.m_pABook, entryID, m_hostorIP.GetBuffer(kMaxCoolAddress));
			m_hostorIP.ReleaseBuffer(-1);
			m_ipaddress = m_hostorIP;
		}
	}
}

void CAddressCooltalk::SetExplanationText()
{
	CString expl;
	switch (m_iUseServer)
	{
		case kDefaultDLS:
			expl = "";
			break;

		case kSpecificDLS:
			expl.LoadString(IDS_EXAMPLESPECIFICDLS);
			break;

		case kHostOrIPAddress:
			expl.LoadString(IDS_EXAMPLEHOSTNAME);
			break;
	}
	SetDlgItemText(IDC_EXPLANATION1, expl);
}

void CAddressCooltalk::OnSelendokServer()
{
	switch (m_iUseServer)
	{
		case kDefaultDLS:
			break;

		case kSpecificDLS:
            GetDlgItem(IDC_IP_ADDRESS)->GetWindowText(m_specificDLS);
			break;

		case kHostOrIPAddress:
            GetDlgItem(IDC_IP_ADDRESS)->GetWindowText(m_hostorIP);
			break;
	}

	UpdateData();
	switch (m_iUseServer)
	{
		case kDefaultDLS:
			GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(FALSE);
			SetDlgItemText(IDC_IP_ADDRESS, "");
			break;

		case kSpecificDLS:
			GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(TRUE);
			SetDlgItemText(IDC_IP_ADDRESS, m_specificDLS);
			break;

		case kHostOrIPAddress:
			GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(TRUE);
			SetDlgItemText(IDC_IP_ADDRESS, m_hostorIP );
			break;
	}
	SetExplanationText();
}

void CAddressCooltalk::OnCloseWindow()
{
	((CAddrEditProperties*) GetParent())->CloseWindow();
}

BOOL CAddressCooltalk::OnInitDialog() 
{
	// TODO: Add your specialized code here and/or call the base class
	CNetscapePropertyPage::OnInitDialog();
	UpdateData (FALSE);
	
	if (m_iUseServer == 0)
		GetDlgItem(IDC_IP_ADDRESS)->EnableWindow(FALSE);
	SetExplanationText();
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());

	return TRUE;
}

BOOL CAddressCooltalk::PerformOnOK(PersonEntry* person) 
{
	// TODO: Add your specialized code here and/or call the base class
	if (m_bActivated)
		UpdateData();

	person->pCoolAddress = m_ipaddress.GetBuffer(0);
	person->UseServer = m_iUseServer;

	return TRUE;
}

BOOL CAddressCooltalk::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	SetFonts (((CAddrEditProperties*) GetParent())->GetHFont());
	m_bActivated = TRUE;
	return(TRUE);
}


////////////////////////////////////////////////////////////////////////////
// CAddrLDAPProperties

CAddrLDAPProperties::CAddrLDAPProperties (CWnd * parent,
		MWContext* context,
		DIR_Server*  dir,
		LPCTSTR lpszCaption) :
    CNetscapePropertySheet ( lpszCaption, parent )
{

		// for New server only
	DIR_InitServer(&m_serverInfo);
	m_serverInfo.dirType = LDAPDirectory;
	m_serverInfo.saveResults = TRUE;
	m_pLDAPProperties = NULL;
	m_pOfflineProperties = NULL;
	m_pExistServer = dir;
	m_context = context;
}

CAddrLDAPProperties::~CAddrLDAPProperties ( )
{
	if (m_pFont) {
		theApp.ReleaseAppFont(m_pFont);
	}
	if ( m_pLDAPProperties )
        delete m_pLDAPProperties;
	if ( m_pOfflineProperties )
        delete m_pOfflineProperties;
}

void CAddrLDAPProperties::OnHelp()
{
	if (GetActivePage() == m_pLDAPProperties)
		NetHelp(HELP_EDIT_USER);
	else if (GetActivePage() == m_pOfflineProperties)
		NetHelp(HELP_EDIT_USER_CONTACT);
}

BEGIN_MESSAGE_MAP(CAddrLDAPProperties, CNetscapePropertySheet)
	//{{AFX_MSG_MAP(CAddrLDAPProperties)
		// NOTE: the ClassWizard will add message map macros here
	    ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CAddrLDAPProperties::DoModal()
{
	if (!m_MailNewsResourceSwitcher.Initialize())
		return -1;
	m_pLDAPProperties = new CServerDialog (this, m_pExistServer, &m_serverInfo);
	m_pOfflineProperties = new CServerOfflineDialog (this, m_pExistServer, &m_serverInfo);
	AddPage( m_pLDAPProperties );
	AddPage( m_pOfflineProperties );

	return CNetscapePropertySheet::DoModal();
}


int CAddrLDAPProperties::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	m_MailNewsResourceSwitcher.Reset();
	if (CNetscapePropertySheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}


BOOL CAddrLDAPProperties::OnInitDialog ( )
{            
	BOOL bResult = TRUE;
	CString label;   

	m_MailNewsResourceSwitcher.Reset();
	int16 guicsid = 0;
	if (m_context)
	{
		INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_context);
		guicsid = INTL_GetCSIWinCSID(csi);
	}	
	else
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
#ifdef _WIN32		
	bResult = CNetscapePropertySheet::OnInitDialog();
#endif

	if (m_pExistServer)
	{
		label.LoadString(IDS_LDAP_SERVER_PROPERTY);
		SetWindowText(LPCTSTR(label));
	}

	return bResult;
}


/****************************************************************************
*
*	CAddrLDAPProperties::OnOK
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We override this function because we are a modeless window.
*
****************************************************************************/


/////////////////////////////////////////////////////////////////////////////
// CServerDialog		   
CServerDialog::CServerDialog(CWnd *pParent, DIR_Server *pExistServer, 
							 DIR_Server *pNewServer)
	: CNetscapePropertyPage(CServerDialog::IDD)
{
	m_pExistServer = pExistServer;

	// for New server only
	m_serverInfo =pNewServer;
	m_bActivated = FALSE;
}

BOOL CServerDialog::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();

	if (m_pExistServer)
	{
		SetDlgItemText(IDC_EDIT_DESCRIPTION, m_pExistServer->description);
		if (m_pExistServer->dirType != PABDirectory)
		{
			SetDlgItemText(IDC_EDIT_SERVER, m_pExistServer->serverName);
			SetDlgItemText(IDC_EDIT_ROOT, m_pExistServer->searchBase);
			SetDlgItemInt(IDC_EDIT_PORT_NO, m_pExistServer->port);
			SetDlgItemInt(IDC_EDIT_MAX_HITS, m_pExistServer->maxHits);
			HG28981
			CheckDlgButton(IDC_SAVE_PASSWORD, m_pExistServer->savePassword);
			CheckDlgButton(IDC_LOGIN_LDAP, m_pExistServer->enableAuth);
		}
		else
		{
			GetDlgItem(IDC_EDIT_SERVER)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_ROOT)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_PORT_NO)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_MAX_HITS)->EnableWindow(FALSE);
			HG72186
			GetDlgItem(IDC_SAVE_PASSWORD)->EnableWindow(FALSE);
			GetDlgItem(IDC_LOGIN_LDAP)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_SERVER)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_ROOT)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_PORT)->EnableWindow(FALSE);
			GetDlgItem(IDC_STATIC_MAX_HITS)->EnableWindow(FALSE);
		}
	}
	else
	{
		SetDlgItemInt(IDC_EDIT_MAX_HITS, 100);
		SetDlgItemInt(IDC_EDIT_PORT_NO, LDAP_PORT);
	}
#ifdef _WIN32
	((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->SetLimitText(MAX_DESCRIPTION_LEN - 1);
	((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->SetLimitText(MAX_HOSTNAME_LEN - 1);
#else
	((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->LimitText(MAX_DESCRIPTION_LEN - 1);
	((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->LimitText(MAX_HOSTNAME_LEN - 1);
	// we aren't doing secure ldap on win16 so hide the checkbox
	
#endif
	((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->SetFocus();
	OnEnableLoginLDAP();
	return 0;
}

BOOL CServerDialog::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}

BOOL CServerDialog::OnKillActive( )
{
	if (!ValidDataInput())
		return FALSE;
	return TRUE;
}


void CServerDialog::OnOK() 
{
    CNetscapePropertyPage::OnOK();

    char text[MAX_DESCRIPTION_LEN];
	if (m_pExistServer)
	{
		if (GetDlgItemText(IDC_EDIT_DESCRIPTION, text, MAX_DESCRIPTION_LEN))
		{
			XP_FREE(m_pExistServer->description);
			m_pExistServer->description = XP_STRDUP(text);
		}
		if (m_pExistServer->dirType == PABDirectory)
			return;
		if (GetDlgItemText(IDC_EDIT_SERVER, text, MAX_DESCRIPTION_LEN))
		{
			XP_FREE(m_pExistServer->serverName);
			m_pExistServer->serverName = XP_STRDUP(text);
		}
		if (GetDlgItemText(IDC_EDIT_ROOT, text, MAX_DESCRIPTION_LEN))
		{
			XP_FREE(m_pExistServer->searchBase);
			m_pExistServer->searchBase = XP_STRDUP(text);
		}
		m_pExistServer->port = (int)GetDlgItemInt(IDC_EDIT_PORT_NO);
		m_pExistServer->maxHits = (int)GetDlgItemInt(IDC_EDIT_MAX_HITS);
		HG90271
		if (IsDlgButtonChecked(IDC_SAVE_PASSWORD))
			m_pExistServer->savePassword = TRUE;
		else
			m_pExistServer->savePassword = FALSE;
		if (IsDlgButtonChecked(IDC_LOGIN_LDAP))
			m_pExistServer->enableAuth = TRUE;
		else
			m_pExistServer->enableAuth = FALSE;
	}
	else
	{
		char port[16];
		if (GetDlgItemText(IDC_EDIT_DESCRIPTION, text, MAX_DESCRIPTION_LEN))
			m_serverInfo->description = XP_STRDUP(text);
		if (GetDlgItemText(IDC_EDIT_SERVER, text, MAX_DESCRIPTION_LEN))
			m_serverInfo->serverName = XP_STRDUP(text);
		if (GetDlgItemText(IDC_EDIT_ROOT, text, MAX_DESCRIPTION_LEN))
			m_serverInfo->searchBase = XP_STRDUP(text);
		HG92177
		if (GetDlgItemText(IDC_EDIT_PORT_NO, port, 16) > 0)
			m_serverInfo->port = atoi(port);
		else
		{
			HG98216
				m_serverInfo->port = LDAP_PORT;
		}
		m_serverInfo->maxHits = (int)GetDlgItemInt(IDC_EDIT_MAX_HITS);
		if (IsDlgButtonChecked(IDC_SAVE_PASSWORD))
			m_serverInfo->savePassword = TRUE;
		else
			m_serverInfo->savePassword = FALSE;
		if (IsDlgButtonChecked(IDC_LOGIN_LDAP))
			m_serverInfo->enableAuth = TRUE;
		else
			m_serverInfo->enableAuth = FALSE;
	}
}

void CServerDialog::OnCheckSecure() 
{
	HG91761
}

void CServerDialog::OnEnableLoginLDAP() 
{
	if (IsDlgButtonChecked(IDC_LOGIN_LDAP))
		GetDlgItem(IDC_SAVE_PASSWORD)->EnableWindow(TRUE);
	else {
		GetDlgItem(IDC_SAVE_PASSWORD)->EnableWindow(FALSE);
		CButton* button = (CButton*) GetDlgItem(IDC_SAVE_PASSWORD);
		button->SetCheck(FALSE);
	}
}

BOOL CServerDialog::ValidDataInput()
{
    char text[MAX_DESCRIPTION_LEN];
	if (0 == GetDlgItemText(IDC_EDIT_DESCRIPTION, text, MAX_DESCRIPTION_LEN))
	{
		CAddrFrame::HandleErrorReturn(0, this, IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_DESCRIPTION))->SetFocus();
		return FALSE;
	}

	if (m_pExistServer && m_pExistServer->dirType == PABDirectory)
		return TRUE;

	if (0 == GetDlgItemText(IDC_EDIT_SERVER, text, MAX_DESCRIPTION_LEN))
	{
		CAddrFrame::HandleErrorReturn(0, this, IDS_EMPTY_STRING);
		((CEdit*)GetDlgItem(IDC_EDIT_SERVER))->SetFocus();
		return FALSE;
	}
	if (GetDlgItemText(IDC_EDIT_PORT_NO, text, MAX_DESCRIPTION_LEN))
	{
		int nPort = GetDlgItemInt(IDC_EDIT_PORT_NO);
		if (nPort < 0 && nPort> MAX_PORT_NUMBER)
		{
			CAddrFrame::HandleErrorReturn(0, this, IDS_PORT_RANGE);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;	    
		}
		if (!::IsNumeric(text))
		{
			CAddrFrame::HandleErrorReturn(0, this, IDS_NUMBERS_ONLY);
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_PORT_NO))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;	    
		}
	}
	if (GetDlgItemText(IDC_EDIT_MAX_HITS, text, MAX_DESCRIPTION_LEN))
	{
		if (!::IsNumeric(text))
		{
			CAddrFrame::HandleErrorReturn(0, this, IDS_NUMBERS_ONLY);
			((CEdit*)GetDlgItem(IDC_EDIT_MAX_HITS))->SetFocus();
			((CEdit*)GetDlgItem(IDC_EDIT_MAX_HITS))->SetSel((DWORD)MAKELONG(0, -1));
			return FALSE;
		}
	}
	return TRUE;
}

void CServerDialog::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
}

void CServerDialog::OnHelp() 
{
	NetHelp(HELP_LDAP_SERVER_PROPS);
}

BEGIN_MESSAGE_MAP(CServerDialog, CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_SECURE, OnCheckSecure)
	ON_BN_CLICKED(IDC_LOGIN_LDAP, OnEnableLoginLDAP)
	ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CServerOfflineDialog		   
CServerOfflineDialog::CServerOfflineDialog(CWnd *pParent, DIR_Server *pExistServer, 
							 DIR_Server *pNewServer)
	:  CNetscapePropertyPage(CServerOfflineDialog::IDD)
{
	m_pExistServer = pExistServer;

	// for New server only
	m_serverInfo = pNewServer;
	m_bActivated = FALSE;
}

BOOL CServerOfflineDialog::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();

	if (m_pExistServer)
	{
		CheckDlgButton(IDC_CHECK1, DIR_TestFlag  (m_pExistServer, DIR_REPLICATION_ENABLED));
	}
	else
	{
		CheckDlgButton(IDC_CHECK1, FALSE);
	}

	return 0;
}

BOOL CServerOfflineDialog::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	if(!CNetscapePropertyPage::OnSetActive()) 
        return(FALSE);

    if(m_bActivated)
        return(TRUE);
	m_bActivated = TRUE;
	return(TRUE);
}

void CServerOfflineDialog::OnOK() 
{
    CNetscapePropertyPage::OnOK();

	if (m_bActivated)
	{
		if (m_pExistServer)
		{
			if (IsDlgButtonChecked(IDC_CHECK1))
				DIR_SetFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
			else
				DIR_ClearFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
		}
		else
		{
			if (IsDlgButtonChecked(IDC_CHECK1))
				DIR_SetFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
			else
				DIR_ClearFlag (m_pExistServer, DIR_REPLICATION_ENABLED);
		}
	}
}

void CServerOfflineDialog::DoDataExchange(CDataExchange* pDX)
{
	CNetscapePropertyPage::DoDataExchange(pDX);
}

void CServerOfflineDialog::OnHelp() 
{
	NetHelp(HELP_LDAP_SERVER_PROPS);
}

void CServerOfflineDialog::OnUpdateNow() 
{

}

BEGIN_MESSAGE_MAP(CServerOfflineDialog, CNetscapePropertyPage)
	ON_BN_CLICKED(IDC_UPDATE_NOW, OnUpdateNow)
	ON_BN_CLICKED(ID_HELP, OnHelp)
END_MESSAGE_MAP()

#endif //MOZ_NEWADDR


