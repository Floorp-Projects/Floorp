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

#include "dialog.h"

#include "helper.h"
#include "nethelp.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" void sample_exit_routine(URL_Struct *URL_s,int status,MWContext *window_id);

static CString url_string = "";
// Retain radio button setting across dialog calls
// 0 = Load URL into Browser, 1 = Editor
static int browse_or_edit = 0;
/////////////////////////////////////////////////////////////////////////////
// CDialogURL dialog


CDialogURL::CDialogURL(CWnd* pParent, MWContext * context)
    : CDialog(CDialogURL::IDD, pParent)
{
    //{{AFX_DATA_INIT(CDialogURL)
    m_csURL = url_string;
    //}}AFX_DATA_INIT
	ASSERT(context);
    m_Context = context;
}


BOOL CDialogURL::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CEdit * pBox = (CEdit *) GetDlgItem(IDC_URL); 

	if(pBox) {
		pBox->SetWindowText((const char *) url_string);
		pBox->SetFocus();
		pBox->SetSel(0, -1);
#ifdef EDITOR
		// Communicator version has radio buttons to select 
		//  loading URL into a browser or editor
		if ( EDT_IS_EDITOR(m_Context) ){
    	    ((CButton *)GetDlgItem(IDC_OPEN_URL_EDITOR))->SetCheck(1);
        } else {
    	    ((CButton *)GetDlgItem(IDC_OPEN_URL_BROWSER))->SetCheck(1);
        }
#endif // EDITOR
		return(0);
	} 
    return(1);
}

BEGIN_MESSAGE_MAP(CDialogURL, CDialog)
	//{{AFX_MSG_MAP(CDialogURL)
	ON_BN_CLICKED(IDC_BROWSE_FILE, OnBrowseForFile)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialogURL message handlers
/////////////////////////////////////////////////////////////////////////////

void CDialogURL::OnBrowseForFile()
{
    int type = HTM;
#ifdef EDITOR
    // Restrict to only *.HTML and allow *.SHTML if loading in Composer
    if( ((CButton *)GetDlgItem(IDC_OPEN_URL_EDITOR))->GetCheck() ){
        type = HTM_ONLY;
    }
#endif
    char * pName = wfe_GetExistingFileName( this->m_hWnd, szLoadString(IDS_OPEN), type, TRUE, NULL);
    if( pName ){
        GetDlgItem(IDC_URL)->SetWindowText(pName);
        XP_FREE(pName);
        // Set focus to Open button so Enter can be used 
        // immediately after choosing file
        GotoDlgCtrl(GetDlgItem(IDOK));
    }
}

void CDialogURL::OnHelp()
{

	NetHelp( HELP_OPEN_PAGE );
}

void CDialogURL::OnOK()
{

	CEdit * pBox = (CEdit *) GetDlgItem(IDC_URL); 

	if(pBox) {
		pBox->GetWindowText(url_string);
	}

	CDialog::OnOK();
#ifdef XP_WIN32
	url_string.TrimLeft();
	url_string.TrimRight();
#endif

	// this was typed in so no referrer -> OK to do an OnNormalLoad
	if(!url_string.IsEmpty()) {

#ifdef EDITOR
        BOOL bEdit = ((CButton *)GetDlgItem(IDC_OPEN_URL_EDITOR))->GetCheck() != 0;

        if (bEdit || EDT_IS_EDITOR(m_Context)) {
            FE_LoadUrl((char*)LPCSTR(url_string), bEdit);
        } else
#endif
            // Load the URL into the same window only if called from an existing browser
    		ABSTRACTCX(m_Context)->NormalGetUrl(url_string);
	}

}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogLicense dialog

CDialogLicense::CDialogLicense(CWnd* pParent /*=NULL*/)
    : CDialog(CDialogLicense::IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(CDialogLicense, CDialog)
END_MESSAGE_MAP()

int CDialogLicense::DoModal()
{
    int status;
    
    status = CDialog::DoModal();

    if(status == IDOK)
        return(TRUE);
    else
        return(FALSE);

}

BOOL CDialogLicense::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CEdit * edit = (CEdit *) GetDlgItem(IDC_EDIT1);  
	if(edit) {
		LPSTR	lpszLicenseText = wfe_LoadResourceString("license");

		if (lpszLicenseText) {
			edit->SetWindowText(lpszLicenseText);
            CDC * pdc = GetDC();
            LOGFONT lf;			// Instead of using ANSI_VAR_FONT for i18n
            XP_MEMSET(&lf,0,sizeof(LOGFONT));
            lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
            lf.lfCharSet = DEFAULT_CHARSET;
	        strcpy(lf.lfFaceName, szLoadString(IDS_FONT_FIXNAME));
   	        lf.lfHeight = -MulDiv(10,pdc->GetDeviceCaps(LOGPIXELSY), 72);
            lf.lfQuality = PROOF_QUALITY;    
            m_cfTextFont.CreateFontIndirect ( &lf );
			edit->SetFont( &m_cfTextFont );
			ReleaseDC(pdc);
			XP_FREE(lpszLicenseText);
		}
	}

	return(1);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogSecurity dialog

CDialogSecurity::CDialogSecurity(int myType, XP_Bool *returnPref, CWnd* pParent /*=NULL*/)
    : CDialog(CDialogSecurity::IDD, pParent)
{   
	if((myType < 0) || (myType > MAX_SECURITY_CHECKS - 1))
		myType = 0;

	m_Type = myType;

	returnpref = returnPref;
}

BEGIN_MESSAGE_MAP(CDialogSecurity, CDialog)
END_MESSAGE_MAP()

int CDialogSecurity::DoModal()
{
    int status;
    
    status = CDialog::DoModal();

    if(status == IDOK)
        return(TRUE);
    else
        return(FALSE);

}

//
// If we've gotten here then obviously the dialog is enabled so
//   turn the little button thingie on and shove the proper text into
//   the edit area
//
BOOL CDialogSecurity::OnInitDialog() 
{   
    Bool      allowTurnOff = TRUE;
    Bool      allowCancel  = TRUE;
	CButton * button;

	CDialog::OnInitDialog();

	if ( returnpref == NULL ) {
		allowTurnOff = FALSE;
	}
	CEdit * edit = (CEdit *) GetDlgItem(IDC_SECUREALERTEDIT);  
	if(edit) {
		switch(m_Type) {
	  	case SD_ENTERING_SECURE_SPACE:
			  edit->SetWindowText(szLoadString(IDS_ENTER_SECURE_0));
			  allowCancel = FALSE;
			  break;
		case SD_LEAVING_SECURE_SPACE:
			  edit->SetWindowText(szLoadString(IDS_LEAVE_SECURE_0));
			  break;
		case SD_INSECURE_POST_FROM_INSECURE_DOC:
			  edit->SetWindowText(szLoadString(IDS_NONSEC_POST_FR_NONSEC_0));
			  break;  
		case SD_INSECURE_POST_FROM_SECURE_DOC:
			  edit->SetWindowText(szLoadString(IDS_NONSEC_POST_FR_SEC_0));
			  break;  
		case SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN:
			  edit->SetWindowText(szLoadString(IDS_NONSEC_INLINES_0));
			  allowCancel = FALSE;
			  break;  
		case SD_REDIRECTION_TO_INSECURE_DOC:
			  edit->SetWindowText(szLoadString(IDS_NONSEC_REDIRECT_0)); 
			  break;  
		case SD_REDIRECTION_TO_SECURE_SITE:
			  edit->SetWindowText(szLoadString(IDS_REDIRECT_TO_SECURE)); 
			  break;  
		default:
		  edit->SetWindowText(szLoadString(IDS_NONSEC_UNKNOWN));
		} 
	}
  
	button = (CButton *) GetDlgItem(IDC_SECURESHOWAGAIN);
	if(button)
	  button->SetCheck(TRUE);  

	// can't turn off redirection warning
	if(!allowTurnOff && button)
		button->ShowWindow(SW_HIDE);


	button = (CButton *) GetDlgItem(IDCANCEL);
	if(!allowCancel)
		button->EnableWindow(FALSE);
  
	return(1);
}

void CDialogSecurity::OnOK()
{ 

	CButton * button = (CButton *) GetDlgItem(IDC_SECURESHOWAGAIN);
	if(button) {
		theApp.m_nSecurityCheck[m_Type] = button->GetCheck(); 
		if ( returnpref ) {
			if ( theApp.m_nSecurityCheck[m_Type] ) {
				*returnpref = TRUE;
			} else {
				*returnpref = FALSE;
			}
		}
	}

	CDialog::OnOK();
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogPRMT dialog

CDialogPRMT::CDialogPRMT(CWnd* pParent /*=NULL*/)
    : CDialog(CDialogPRMT::IDD, pParent)
{
    m_csTitle = _T("");
    
    //{{AFX_DATA_INIT(CDialogPRMT)
    m_csAns = "";
    //}}AFX_DATA_INIT
}

void CDialogPRMT::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CDialogPRMT)
    DDX_Text(pDX, IDC_PROMPT_ASK, m_csAsk);
    DDX_Text(pDX, IDC_PROMPT_ANS, m_csAns);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDialogPRMT, CDialog)
    //{{AFX_MSG_MAP(CDialogPRMT)
        // NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialogPRMT message handlers

char * CDialogPRMT::DoModal(const char * Msg, const char * Dflt, const char *pszCaption)
{
    int status;
    if(!m_csAsk.IsEmpty()) 
        m_csAsk.Empty();

    if(!m_csAns.IsEmpty())
        m_csAns.Empty();

    m_csAsk = Msg;
    m_csAns = Dflt;
	if (pszCaption)
		m_csCaption = pszCaption;
	else
		m_csCaption.Format(szLoadString(IDS_USER_PROMPT), szLoadString(AFX_IDS_APP_TITLE));


    status = CDialog::DoModal();

    if(status == IDOK)
        return(XP_STRDUP((const char *) m_csAns));
    else    
        return(NULL);
}                        

int CDialogPRMT::OnInitDialog()
{   
    if( !m_csTitle.IsEmpty() )
        SetWindowText( (LPCSTR)m_csTitle );
    else
    	SetWindowText(m_csCaption);   
    
	CWnd *pWnd = GetDlgItem(IDC_PROMPT_ASK);
	if (pWnd)
		pWnd->SetWindowText(m_csAsk);

	CEdit *edit = (CEdit *) GetDlgItem(IDC_PROMPT_ANS);
	if (edit) {
		edit->SetWindowText(m_csAns);
		edit->SetFocus();
		edit->SetSel(0, -1);
		return(0);
	}

	return(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogPASS dialog

CDialogPASS::CDialogPASS(CWnd* pParent /*=NULL*/)
    : CDialog(CDialogPASS::IDD, pParent)
{
    m_csTitle = _T("");
    
    //{{AFX_DATA_INIT(CDialogPASS)
    m_csAns = "";
    //}}AFX_DATA_INIT
}

void CDialogPASS::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CDialogPASS)
    DDX_Text(pDX, IDC_PROMPT_ASK, m_csAsk);
    DDX_Text(pDX, IDC_PROMPT_ANS, m_csAns);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDialogPASS, CDialog)
    //{{AFX_MSG_MAP(CDialogPASS)
        // NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialogPASS message handlers

char * CDialogPASS::DoModal(const char * Msg)
{
    int status;
    if(!m_csAsk.IsEmpty()) 
        m_csAsk.Empty();

    if(!m_csAns.IsEmpty())
        m_csAns.Empty();

    m_csAsk = Msg;
    status = CDialog::DoModal();

    if(status == IDOK)
        return(XP_STRDUP((const char *) m_csAns));
    else    
        return(NULL);
}
                                                             
                                                             
int CDialogPASS::OnInitDialog()
{

    CDialog::OnInitDialog();

#ifndef XP_WIN32
	//win16 only
	if ( m_hWnd ) {
		BringWindowToTop();
		SetWindowPos( &wndTopMost, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	}
#endif

    if( !m_csTitle.IsEmpty() )
        SetWindowText( (LPCSTR)m_csTitle );
    
	CEdit * edit = (CEdit *) GetDlgItem(IDC_PROMPT_ANS);

	if(edit) {
		edit->SetFocus();
		edit->SetSel(0, -1);
		return(0);
	}

	return(1);

}

                                                             
/////////////////////////////////////////////////////////////////////////////
// CDialogUPass dialog


CDialogUPass::CDialogUPass(CWnd* pParent /*=NULL*/)
  : CDialog(CDialogUPass::IDD, pParent)
{
  //{{AFX_DATA_INIT(CDialogUPass)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}

void CDialogUPass::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialogUPass)
	DDX_Text(pDX, IDC_PROMPT, m_csMessage);
	DDX_Text(pDX, IDC_EDIT1, m_csUser);
	DDX_Text(pDX, IDC_EDIT2, m_csPasswd);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDialogUPass, CDialog)
  //{{AFX_MSG_MAP(CDialogUPass)
    // NOTE: the ClassWizard will add message map macros here
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CDialogUPass message handlers

int CDialogUPass::DoModal(char * message, char ** user, char ** passwd)
{

	int status;

	if(!user || !passwd)
		return(FALSE);

	if(message)
		m_csMessage = message;
	else
		m_csMessage = szLoadString(IDS_AUTH_DEFAULT);

	if(*user)
		m_csUser   = *user;
    else
        m_csUser   = "";

#if defined(SingleSignon)
    if(*passwd)
	m_csPasswd = *passwd;
    else
#endif
    m_csPasswd = "";

	status = CDialog::DoModal();

	if(status != IDOK)
		return(FALSE);
  
	*user   = XP_STRDUP((const char *) m_csUser);
	*passwd = XP_STRDUP((const char *) m_csPasswd);

	return(TRUE);

}

int CDialogUPass::OnInitDialog()
{
    CDialog::OnInitDialog();

#ifndef XP_WIN32
	//win16 only
	if ( m_hWnd ) {
		BringWindowToTop();
		SetWindowPos( &wndTopMost, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	}
#endif

    if( !m_csTitle.IsEmpty() )
        SetWindowText( (LPCSTR)m_csTitle );
    
	return(1);
}


/////////////////////////////////////////////////////////////////////////////
// CUnknownTypeDlg dialog


CUnknownTypeDlg::CUnknownTypeDlg(CWnd* pParent /*=NULL*/, char * filetype,CHelperApp * app)
  : CDialog(CUnknownTypeDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CUnknownTypeDlg)
	m_FileType = filetype;
	m_app = app;
	//}}AFX_DATA_INIT
}

void CUnknownTypeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUnknownTypeDlg)
	DDX_Text(pDX, IDC_FILETYPE, m_FileType);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CUnknownTypeDlg, CDialog)
	//{{AFX_MSG_MAP(CUnknownTypeDlg)
	ON_BN_CLICKED(ID_CONFIGUREVIEWER, OnConfigureviewer)
	ON_BN_CLICKED(ID_SAVETODISK, OnSavetodisk)
	ON_BN_CLICKED(IDC_MORE_INFO, OnMoreInfo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CUnknownTypeDlg message handlers

void CUnknownTypeDlg::OnConfigureviewer()
{
	CConfigureViewerSmall dlg(this, (const char *)m_FileType, m_app);

	if (dlg.DoModal() == IDOK) {
		//	Ensure the app points to what the user actually typed in.
		m_app->csCmd = dlg.m_AppName;
		m_app->bChanged = TRUE;
		m_app->how_handle = HANDLE_EXTERNAL;
		EndDialog(HANDLE_EXTERNAL);
	}
}

void CUnknownTypeDlg::OnSavetodisk()
{
    EndDialog(HANDLE_SAVE); 
}

void CUnknownTypeDlg::OnCancel()
{
    CDialog::OnCancel();
}

void CUnknownTypeDlg::OnMoreInfo()
{
    EndDialog(HANDLE_MOREINFO); 
}
/////////////////////////////////////////////////////////////////////////////
// CNewMimeType dialog


CNewMimeType::CNewMimeType(CWnd* pParent /*=NULL*/)
  : CDialog(CNewMimeType::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewMimeType)
	m_MimeSubtype = "";
	m_MimeType = "";
	//}}AFX_DATA_INIT
}

void CNewMimeType::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewMimeType)
	DDX_Text(pDX, IDC_MIMESUBTYPE_EDIT, m_MimeSubtype);
	DDX_Text(pDX, IDC_MIMETYPE_EDIT, m_MimeType);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CNewMimeType, CDialog)
  //{{AFX_MSG_MAP(CNewMimeType)
    // NOTE: the ClassWizard will add message map macros here
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CNewMimeType message handlers
/////////////////////////////////////////////////////////////////////////////
// CConfigureViewerSmall dialog


CConfigureViewerSmall::CConfigureViewerSmall(CWnd* pParent /*=NULL*/, const char * filetype,  CHelperApp * app)
  : CDialog(CConfigureViewerSmall::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigureViewerSmall)
	m_MimeType = filetype;
	m_AppName = _T("");
	//}}AFX_DATA_INIT

	m_app = app;
}

void CConfigureViewerSmall::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigureViewerSmall)
	DDX_Text(pDX, IDC_MIMETYPE_EDIT, m_MimeType);
	DDX_Text(pDX, IDC_HELPER_PATH, m_AppName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CConfigureViewerSmall, CDialog)
  //{{AFX_MSG_MAP(CConfigureViewerSmall)
  ON_BN_CLICKED(IDC_HELPER_BROWSE, OnHelperBrowse)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CConfigureViewerSmall message handlers

void CConfigureViewerSmall::OnHelperBrowse()
{
	char * name;

	name = wfe_GetExistingFileName(m_hWnd,
	                      szLoadString(IDS_SELECT_APPROPRIATE_VIEWER), EXE, TRUE);

	// user selected a file
	if(name) {
		CStatic   * pIcon = (CStatic *)GetDlgItem(IDC_HELPER_STATIC4);
		HICON hIcon;

		//	NT can't handle paths with spaces
		//	We don't deal with it here, so it's up to the user.
		//	If they quote it in their registry, then we preserve their string.

		//	Force redraw of the name they enetered.
		CEdit *pEdit = (CEdit *)GetDlgItem(IDC_HELPER_PATH);
		if(pEdit)	{
			pEdit->SetWindowText(name);
		}

		hIcon = ExtractIcon(theApp.m_hInstance,(const char *)m_AppName,0);
		pIcon->SetIcon(hIcon);
		XP_FREE(name);    
	} 
}


/****************************************************************************
*
*	Class: CDefaultBrowserDlg
*
*	DESCRIPTION:
*		This provides a dialog for notifying the user that another application
*		has made themselves the "default browser" by changing our registry
*		entries.
*
****************************************************************************/

BEGIN_MESSAGE_MAP(CDefaultBrowserDlg, CDefaultBrowserDlgBase)
	//{{AFX_MSG_MAP(CDefaultBrowserDlg)
	ON_BN_CLICKED(ID_NO, OnNo)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/****************************************************************************
*
*	CDefaultBrowserDlg::CDefaultBrowserDlg
*
*	PARAMETERS:
*		pParent	- pointer to parent window (= NULL)
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Standard dialog constructor.
*
****************************************************************************/

CDefaultBrowserDlg::CDefaultBrowserDlg(CWnd* pParent /*=NULL*/)
	: CDefaultBrowserDlgBase(CDefaultBrowserDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDefaultBrowserDlg)
	m_bIgnore = FALSE;
	//}}AFX_DATA_INIT
	
} // END OF	FUNCTION CDefaultBrowserDlg::CDefaultBrowserDlg()

/****************************************************************************
*
*	CDefaultBrowserDlg::DoDataExchange
*
*	PARAMETERS:
*		pDX	- the usual CDataExchange pointer
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Standard dialog data exchange function.
*
****************************************************************************/

void CDefaultBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
	CDefaultBrowserDlgBase::DoDataExchange(pDX);
	
	//{{AFX_DATA_MAP(CDefaultBrowserDlg)
	DDX_Check(pDX, IDC_IGNORE, m_bIgnore);
	DDX_Control(pDX, IDC_LIST1, m_Listbox);
	//}}AFX_DATA_MAP
	
} // END OF	FUNCTION CDefaultBrowserDlg::DoDataExchange()

/****************************************************************************
*
*	CDefaultBrowserDlg::OnNo
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This handles the 'No' button. We treat it just like OnOK, except
*		for the return value.
*
****************************************************************************/

void CDefaultBrowserDlg::OnNo() 
{
	if (UpdateData(TRUE))
	{
		EndDialog(ID_NO);
	}  /* end if */
	
} // END OF	FUNCTION CDefaultBrowserDlg::OnNo()

/****************************************************************************
*
*	CDefaultBrowserDlg::OnPaint
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We use this WM_PAINT handler to draw the standard Windows question
*		icon on the dialog, so it looks more like a message box.
*
****************************************************************************/

void CDefaultBrowserDlg::OnPaint() 
{
	CPaintDC dc(this);
	
	// Draw the Windows question icon in the placeholder
	CWnd * pwndIcon = GetDlgItem(IDC_WARNING_ICON);
	ASSERT(pwndIcon != NULL);
	if (pwndIcon != NULL)
	{
		CRect rc;
		pwndIcon->GetWindowRect(rc);
		ScreenToClient(rc);
		dc.DrawIcon(rc.TopLeft(), ::LoadIcon(NULL, IDI_EXCLAMATION));
	}  /* end if */
	
	// Do not call CDefaultBrowserDlgBase::OnPaint() for painting messages
	
} // END OF	FUNCTION CDefaultBrowserDlg::OnPaint()

BOOL CDefaultBrowserDlg::OnInitDialog()
{
	BOOL res = CDialog::OnInitDialog();

	// Fill in the dialog's list box
	CPtrArray* lostList = theApp.m_OwnedAndLostList.GetLostList();
	
	// Add to list
	int size = lostList->GetSize();
	for (int i = 0; i < size; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)((*lostList)[i]);

		if (!theItem->m_bIgnored)
		{
			theItem->FetchPrettyName();
			if (theItem->m_csPrettyName != "")
			{
				int index = m_Listbox.AddString(theItem->m_csPrettyName);
				m_Listbox.SetItemDataPtr(index, theItem);
			}
		}

		m_Listbox.SetSel(-1);
	}

	return res;
}

static BOOL IsListItemSelected(int* selArray, int count, int i)
{
	for (int x = 0; x < count; x++)
	{
		int item = selArray[x];
		if (i == item)
			return TRUE;
	}
	return FALSE;
}

void CDefaultBrowserDlg::OnOK()
{
	// Let's do it. Selected items become owned.
	// Unselected items become ignored (if the checkbox is checked)
	CDefaultBrowserDlgBase::OnOK();

	int count = m_Listbox.GetCount();
	int* selArray = new int[count];

	int nSelectedArraySize = m_Listbox.GetSelItems(count, selArray);
		
	for (int i = 0; i < count; i++)
	{
		COwnedLostItem* theItem = (COwnedLostItem*)(m_Listbox.GetItemDataPtr(i));
		
		if (IsListItemSelected(selArray, count, i))
		{
			// Remove it from the lost list.
			theApp.m_OwnedAndLostList.RemoveFromLostList(theItem->m_csMimeType);
			
			// Add it to the owned list.
			theApp.m_OwnedAndLostList.GetOwnedList()->Add(theItem);

			// Modify the HelperApp information (write to the registry)
			// Fun fun fun!
			theItem->GiveControlToNetscape();
		}
		else if (m_bIgnore)
		{
			// User wants to ignore this item. Add it to the ignore list.
			theItem->SetIgnored(TRUE);
		}
	}	
}
