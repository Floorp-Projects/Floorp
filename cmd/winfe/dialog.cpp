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

#include "nsIDefaultBrowser.h"
#include "prefapi.h"
#include "edt.h" // For EDT_GetEditHistory and MAX_EDT_HISTORY_LOCATIONS

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" void sample_exit_routine(URL_Struct *URL_s,int status,MWContext *window_id);

// Last URL entered in the Open Page dialog
static CString url_string = "";

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
    m_bInitNavComboBox = FALSE;
    m_bInternalChange = FALSE;

#ifdef EDITOR
    m_bInitComposerComboBox = FALSE;
#endif
    XP_MEMSET(m_pNavTitleList, 0, MAX_HISTORY_LOCATIONS*sizeof(char*));
}

CDialogURL::~CDialogURL() 
{
    for( int i = 0; i < MAX_HISTORY_LOCATIONS; i++ )
        XP_FREEIF(m_pNavTitleList[i]);

    // Note: Composer history list points to other static strings,
    //  don't delete here
}


#ifndef EDITOR
#define MOVE_CONTROL_AMOUNT 30
static void wfe_MoveControl(CDialog *pDialog, UINT nID)
{
    CRect cRect;
    CWnd *pWnd = pDialog->GetDlgItem(nID);
    if( pWnd )
    {
        pWnd->GetWindowRect(&cRect);
        pDialog->ScreenToClient(&cRect);
        pWnd->SetWindowPos(0, cRect.left, cRect.top-MOVE_CONTROL_AMOUNT, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}
#endif

BOOL CDialogURL::OnInitDialog() 
{
	CDialog::OnInitDialog();
	CComboBox *pNavBox = (CComboBox*)GetDlgItem(IDC_URL);
    if( !pNavBox )
        return(1);

    // Used a separate define in dialog.h to avoid having to include edt.h for MAX_EDIT_HISTORY_LOCATIONS
    XP_ASSERT(MAX_HISTORY_LOCATIONS <= MAX_EDIT_HISTORY_LOCATIONS); 

#ifdef EDITOR
	CComboBox *pComposerBox = (CComboBox *) GetDlgItem(IDC_URL_EDITOR); 
    if( !pComposerBox )
        return(1);

    if( EDT_IS_EDITOR(m_Context) )
    {
        // Default is to open into Composer
    	((CButton *)GetDlgItem(IDC_OPEN_URL_EDITOR))->SetCheck(1);
        // Hide Navigator's editbox
        pNavBox->ShowWindow(SW_HIDE);

        // This will init the dropdown list
        GetComposerComboBox();

        pComposerBox->SetFocus();
        pComposerBox->SetEditSel(0, -1);
    }
    else
    {
        // Default is open in Navigator
    	((CButton *)GetDlgItem(IDC_OPEN_URL_BROWSER))->SetCheck(1);
        // Hide the editor's combobox
        pComposerBox->ShowWindow(SW_HIDE);

        // This will init the dropdown list
        GetNavComboBox();

        pNavBox->SetFocus();
        pNavBox->SetEditSel(0, -1);
    }
	return(0);
#else
    GetNavComboBox();
	pNavBox->SetFocus();
	pNavBox->SetSel(0, -1);

    // Move all controls up and resize the dialog as well
    // We could have made a different dialog, but that makes it more difficult
    //  for I18N, so just move the controls instead
    wfe_MoveControl(this, IDC_ENTER_URL_MSG);
    wfe_MoveControl(this, IDC_URL);
    wfe_MoveControl(this, IDC_BROWSE_FILE);
    wfe_MoveControl(this, IDOK);
    wfe_MoveControl(this, IDCANCEL);
    wfe_MoveControl(this, ID_HELP);
    CRect cRect;
    GetWindowRect(&cRect);
    SetWindowPos(0, 0, 0, cRect.Width(), cRect.Height()-MOVE_CONTROL_AMOUNT, SWP_NOMOVE | SWP_NOZORDER);
#endif // EDITOR
	return(0);
}

BEGIN_MESSAGE_MAP(CDialogURL, CDialog)
	//{{AFX_MSG_MAP(CDialogURL)
	ON_BN_CLICKED(IDC_BROWSE_FILE, OnBrowseForFile)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_CBN_EDITCHANGE(IDC_URL, OnChangeNavLocation)
	ON_CBN_SELCHANGE(IDC_URL, OnSelchangeNavList)
	//}}AFX_MSG_MAP
#ifdef EDITOR
	ON_BN_CLICKED(IDC_OPEN_URL_BROWSER, OnOpenInBrowser)
	ON_BN_CLICKED(IDC_OPEN_URL_EDITOR, OnOpenInEditor)
	ON_CBN_SELCHANGE(IDC_URL_EDITOR, OnSelchangeComposerList)
	ON_CBN_EDITCHANGE(IDC_URL_EDITOR, OnChangeComposerLocation)
#endif
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
        GetDlgItem(EDT_IS_EDITOR(m_Context) ? IDC_URL_EDITOR: IDC_URL)->SetWindowText(pName);
        XP_FREE(pName);
        // New behavior - immediately end dialog with filename selected
        OnOK();
    }
}

void CDialogURL::OnHelp()
{

	NetHelp( HELP_OPEN_PAGE );
}

void CDialogURL::OnOK()
{

	CWnd *pNavBox = GetNavComboBox();

#ifdef EDITOR
    BOOL bEdit = ((CButton *)GetDlgItem(IDC_OPEN_URL_EDITOR))->GetCheck() != 0;
    CWnd *pComposerBox = GetComposerComboBox();
    if( bEdit ) {
		pComposerBox->GetWindowText(url_string);
    } else
		pNavBox->GetWindowText(url_string);
#else
	if(pNavBox) {
		pNavBox->GetWindowText(url_string);
	}
#endif

	CDialog::OnOK();
#ifdef XP_WIN32
	url_string.TrimLeft();
	url_string.TrimRight();
#endif

	// this was typed in so no referrer -> OK to do an OnNormalLoad
	if(!url_string.IsEmpty())
    {
        // Bug 36087: Convert relative URL strings to an absolute URL based on current location
        char *pAbsoluteURL = (char *)LPCSTR(url_string);

        int  iType = NET_URL_Type(pAbsoluteURL);

        CString csTemp;
        if( iType == 0 )
        {
            WFE_ConvertFile2Url( csTemp, pAbsoluteURL );
            pAbsoluteURL = (char*)LPCSTR(csTemp);
        }

        BOOL bFreeString = FALSE;
        BOOL bIsFile = NET_IsLocalFileURL(pAbsoluteURL);

        History_entry * pEntry = SHIST_GetCurrent(&m_Context->hist);
        if( pEntry && pEntry->address ) {
            pAbsoluteURL = NET_MakeAbsoluteURL(pEntry->address, pAbsoluteURL );
            bFreeString = TRUE;            
        }

#ifdef EDITOR
        if (bEdit || EDT_IS_EDITOR(m_Context)) {
            // This creates a new edit or browser window
            FE_LoadUrl((char *)LPCSTR(url_string), bEdit);
        } else
#endif
            // Load the URL into the same window only if called from an existing browser
    		ABSTRACTCX(m_Context)->NormalGetUrl(url_string);

        if( pAbsoluteURL && bFreeString )
            XP_FREE(pAbsoluteURL);
	}
}

CComboBox *CDialogURL::GetNavComboBox()
{
    CComboBox *pComboBox = (CComboBox*)GetDlgItem(IDC_URL); 
    
    // Use flag to init once only when we need to
    if( m_bInitNavComboBox )
        return pComboBox;

    m_bInitNavComboBox = TRUE;

    CDC *pDC = pComboBox->GetDC();
    // Get the size of the strings added in the dropdown...
    CSize cSize;
    int iMaxWidth = 0;
    int wincsid = INTL_CharSetNameToID(INTL_ResourceCharSet());

    // Fill the combobox with Composer's "Recent Files" list
    char * pUrl = NULL;
    int j = 0; // Separate counter for title array

// We would like to get Browser history items from the new history system (RDF store?)

    // Use this for the current Browser History (same as in the Go menu)
    // Get the session history list
    XP_List* pList = SHIST_GetList(m_Context);
    if( pList )
    {
        // Get the pointer to the current history entry
	    for( int i = 0; i < MAX_HISTORY_LOCATIONS; i++, pList = pList->prev )
	    {
            //pList = pList->prev;
            if( !pList )
                break;

		    History_entry*  pEntry = (History_entry*)pList->object;
		    //ASSERT(pEntry);
		    if( !pEntry )
                continue;

            // Don't include current page's URL
            if( pEntry == m_Context->hist.cur_doc_ptr )
                continue;

            pComboBox->AddString(pEntry->address);
            m_pNavTitleList[j] = XP_STRDUP(pEntry->title);

            CString csTemp(pUrl);
            if ( pDC )
            {
    		    cSize = CIntlWin::GetTextExtent(wincsid, pDC->GetSafeHdc(), csTemp, csTemp.GetLength());
	            pDC->LPtoDP(&cSize);
	    	    if ( cSize.cx > iMaxWidth )
    			    iMaxWidth = cSize.cx;
    	    }
            j++;
	    }
    }

    if( pComboBox->GetDroppedWidth() < iMaxWidth )
        pComboBox->SetDroppedWidth(iMaxWidth);
    
    // Initialize the edit field with the first history item
    //   or last-used global string
    if( pComboBox->GetCount() > 0 )
    {
        pComboBox->SetCurSel(0);
        SetCaption(m_pNavTitleList[0]);
    }
    else
    {
        pComboBox->SetWindowText((const char*)url_string);
        SetCaption();
    }
    return pComboBox;
}

void CDialogURL::SetCaption(char *pPageTitle)
{
    CString csCaption(szLoadString(pPageTitle ? IDS_OPEN_FILE : IDS_OPEN_PAGE));
    if( pPageTitle )
    {
        // Append the page title so caption is "Open: My Page Title"
        csCaption += pPageTitle;
    }
    SetWindowText((const char*)csCaption);
}

void CDialogURL::OnChangeNavLocation()
{
    // Any text typed in the edit box invalidates the page title
    //  show in the dialog caption, 
    //  but search the title list to find a match
    if( !m_bInternalChange )
    {
    	CString csString;
        CComboBox *pComboBox = GetNavComboBox();
        pComboBox->GetWindowText(csString);
        csString.TrimLeft();
        csString.TrimRight();
        char *pCaption = NULL;
        if( !csString.IsEmpty() )
        {
            // Find user-typed string in the combobox
            //  and set corresponding title
            int nIndex = pComboBox->FindStringExact(0,csString);
            if( nIndex >= 0 )
                pCaption = m_pNavTitleList[nIndex];
        }
        SetCaption(pCaption);
    }
}

void CDialogURL::OnSelchangeNavList()
{
    if( !m_bInternalChange )
        SetCaption(m_pNavTitleList[GetNavComboBox()->GetCurSel()]);
}


#ifdef EDITOR
CComboBox *CDialogURL::GetComposerComboBox()
{
    CComboBox *pComboBox = (CComboBox*)GetDlgItem(IDC_URL_EDITOR); 
    
    // Use flag to init once only when we need to
    if( m_bInitComposerComboBox )
        return pComboBox;

    m_bInitComposerComboBox = TRUE;

    CDC *pDC = pComboBox->GetDC();
    // Get the size of the strings added in the dropdown...
    CSize cSize;
    int iMaxWidth = 0;
    int wincsid = INTL_CharSetNameToID(INTL_ResourceCharSet());

    // Fill the combobox with Composer's "Recent Files" list
    char * pUrl = NULL;
    char * pTitle = NULL;
    int j = 0; // Separate counter for title array
	for( int i = 0; i < MAX_EDIT_HISTORY_LOCATIONS; i++ )
	{
        // Save the Page Title for each URL as well
        // NOTE: We don't have to free these - static list is in xp edit code
		m_pComposerTitleList[i] = 0;
        if(EDT_GetEditHistory(m_Context, i, &pUrl, &m_pComposerTitleList[j]))
        {
            pComboBox->AddString(pUrl);
            CString csTemp(pUrl);
            if ( pDC ){            
    		    cSize = CIntlWin::GetTextExtent(wincsid, pDC->GetSafeHdc(), csTemp, csTemp.GetLength());
	        	pDC->LPtoDP(&cSize);
	    	    if ( cSize.cx > iMaxWidth ){
    			    iMaxWidth = cSize.cx;
		        }
    		}
            j++;
        }
    }
    // ...so we can be sure it's visible by using wider dropdown width
    iMaxWidth += 4;
    if( pComboBox->GetDroppedWidth() < iMaxWidth )
        pComboBox->SetDroppedWidth(iMaxWidth);
    
    // Initialize the edit field with the first history item
    //   or last-used global string (shared with Navigator)
    if( pComboBox->GetCount() > 0 )
    {
        pComboBox->SetCurSel(0);
        SetCaption(m_pComposerTitleList[0]);
    }
    else
    {
        pComboBox->SetWindowText((const char*)url_string);
        SetCaption();
    }

    return pComboBox;
}

void CDialogURL::OnSelchangeComposerList()
{
    if( !m_bInternalChange )
        SetCaption(m_pComposerTitleList[GetComposerComboBox()->GetCurSel()]);
}

void CDialogURL::OnChangeComposerLocation()
{
    // Any text typed in the edit box invalidates the page title
    //  show in the dialog caption, but search the title list
    if( !m_bInternalChange )
    {
    	CString csString;
        CComboBox *pComboBox = GetComposerComboBox();
        pComboBox->GetWindowText(csString);
        csString.TrimLeft();
        csString.TrimRight();
        char *pCaption = NULL;
        if( !csString.IsEmpty() )
        {
            // Find user-typed string in the combobox
            //  and set corresponding title
            int nIndex = pComboBox->FindStringExact(0,csString);
            if( nIndex >= 0 )
                pCaption = m_pComposerTitleList[nIndex];
        }
        SetCaption(pCaption);
    }
}

void CDialogURL::OnOpenInBrowser()
{
	CComboBox *pComposerComboBox = GetComposerComboBox();
    CWnd *pNavComboBox = GetNavComboBox();
    CString csString;
    pComposerComboBox->GetWindowText(csString);
    csString.TrimLeft();
    csString.TrimRight();
    if( !csString.IsEmpty() )
    {
        m_bInternalChange = TRUE;
        pNavComboBox->SetWindowText(csString);
        m_bInternalChange = FALSE;
    }
    pComposerComboBox->ShowWindow(SW_HIDE);
    pNavComboBox->ShowWindow(SW_SHOW);
}

void CDialogURL::OnOpenInEditor()
{
	CComboBox *pComposerComboBox = GetComposerComboBox();
    CWnd *pNavComboBox = GetNavComboBox();
    CString csString;
    pNavComboBox->GetWindowText(csString);
    csString.TrimLeft();
    csString.TrimRight();
    if( !csString.IsEmpty() )
    {
        m_bInternalChange = TRUE;
        pComposerComboBox->SetWindowText(csString);
        m_bInternalChange = FALSE;
    }
    pNavComboBox->ShowWindow(SW_HIDE);
    pComposerComboBox->ShowWindow(SW_SHOW);
}
#endif //EDITOR

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
    ON_BN_CLICKED(IDC_SHOW_DESKTOP_PREFS,OnDetails)
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

CDefaultBrowserDlg::CDefaultBrowserDlg(CWnd* pParent /*=NULL*/, nsIDefaultBrowser* pDefaultBrowser /*=NULL*/)
	: CDefaultBrowserDlgBase(CDefaultBrowserDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDefaultBrowserDlg)
	m_bPerformCheck = TRUE; // If false, we wouldn't be here, now would we?
    m_pDefaultBrowser = pDefaultBrowser;
    if ( m_pDefaultBrowser ) {
        m_pDefaultBrowser->AddRef();
    }
	//}}AFX_DATA_INIT
	
} // END OF	FUNCTION CDefaultBrowserDlg::CDefaultBrowserDlg()

CDefaultBrowserDlg::CDefaultBrowserDlg(nsIDefaultBrowser* pDefaultBrowser)
    : CDefaultBrowserDlgBase(CDefaultBrowserDlg::IDD, NULL)
{
    m_bPerformCheck = TRUE; // If false, we wouldn't be here, now would we?
    m_pDefaultBrowser = pDefaultBrowser;
    if ( m_pDefaultBrowser ) {
        m_pDefaultBrowser->AddRef();
    }
}

CDefaultBrowserDlg::~CDefaultBrowserDlg() {
    if ( m_pDefaultBrowser ) {
        m_pDefaultBrowser->Release();
    }
}

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
	DDX_Check(pDX, IDC_IGNORE, m_bPerformCheck);
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

        // Update "perform check" preference per check-box setting.
        if ( !m_bPerformCheck ) {
            PREF_SetBoolPref("browser.wfe.ignore_def_check",TRUE);
        }
    
	}  /* end if */
	
} // END OF	FUNCTION CDefaultBrowserDlg::OnNo()

/****************************************************************************
*
*	CDefaultBrowserDlg::OnDetails
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This handles the 'Details...' button. We treat it just like OnOK, except
*		for the return value.
*
****************************************************************************/

void CDefaultBrowserDlg::OnDetails() 
{
	if (UpdateData(TRUE))
	{
		EndDialog(IDC_SHOW_DESKTOP_PREFS);

	}  /* end if */
	
} // END OF	FUNCTION CDefaultBrowserDlg::OnDetails()

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
    // Dismiss the dialog.
    CDefaultBrowserDlgBase::OnOK();

    // Update "perform check" preference per check-box setting.
    if ( !m_bPerformCheck ) {
        PREF_SetBoolPref("browser.wfe.ignore_def_check",TRUE);
    }

    if ( m_pDefaultBrowser ) {
        // synchronize registry with these preferences.
        nsresult result = m_pDefaultBrowser->HandlePerPreferences();
        ASSERT( result == NS_OK );
    }

#if 0 // Old code.
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
#endif
}

/* CCheckConfirmDialog: a generic "confirm" dialog including a checkbox.
   It's used by XP.
 */

BEGIN_MESSAGE_MAP(CCheckConfirmDialog, CSelfAdjustingDialog)
END_MESSAGE_MAP()

CCheckConfirmDialog::CCheckConfirmDialog (CWnd *pParent,
	const char *pMessage, const char *pCheckMessage,
	const char *pOKMessage, const char *pCancelMessage,
	BOOL checked) :

	CSelfAdjustingDialog(CCheckConfirmDialog::IDD, pParent),
	mMessage(pMessage), mCheckMessage(pCheckMessage),
	mOKMessage(pOKMessage), mCancelMessage(pCancelMessage)
{
	mCheckState = checked ? 1 : 0;
}


BOOL CCheckConfirmDialog::OnInitDialog()
{
	CSelfAdjustingDialog::OnInitDialog();

	CStatic *messageItem = (CStatic *) GetDlgItem(IDC_STATIC1);
	CButton *checkItem = (CButton *) GetDlgItem(IDC_CHECK1),
			*okButton = (CButton *) GetDlgItem(IDOK),
			*cancelButton = (CButton *) GetDlgItem(IDCANCEL);
	POINT	windRectDiff,
			border;

	// set subwindows' text
	messageItem->SetWindowText((const char *) mMessage);
	checkItem->SetWindowText((const char *) mCheckMessage);
	if (!mOKMessage.IsEmpty())
		okButton->SetWindowText((const char *) mOKMessage);
	if (!mCancelMessage.IsEmpty())
		cancelButton->SetWindowText((const char *) mCancelMessage);
	checkItem->SetCheck(mCheckState);

	// adjust sizes to match text
	CheckOverallSize(&border, FALSE);
	ResizeItemToFitText(messageItem, (const char *) mMessage, &windRectDiff);
	AdjustForItemSize(messageItem, &windRectDiff);
	ResizeItemToFitText(checkItem, (const char *) mCheckMessage, &windRectDiff);
	AdjustForItemSize(checkItem, &windRectDiff);
	AdjustButtons(okButton, cancelButton, border.x);
	CheckOverallSize(&border, TRUE);

	return TRUE;
}

BOOL CCheckConfirmDialog::DoModal(XP_Bool *checkboxSet)
{
	BOOL	rtnval = CSelfAdjustingDialog::DoModal() == IDOK;
	*checkboxSet = mCheckState == 1;
	return rtnval;
}

void CCheckConfirmDialog::OnOK()
{
	CButton	*checkItem = (CButton *) GetDlgItem(IDC_CHECK1);
	mCheckState = checkItem->GetCheck() == 1;
	CSelfAdjustingDialog::OnOK();
}

/* "cancel" really means "no," so fetch the value of the checkbox */
void CCheckConfirmDialog::OnCancel()
{
	CButton	*checkItem = (CButton *) GetDlgItem(IDC_CHECK1);
	mCheckState = checkItem->GetCheck() == 1;
	CSelfAdjustingDialog::OnCancel();
}

/* if (!adjust), calculate the border around our subwindows.
   if (adjust), resize to have the given border. */
void CCheckConfirmDialog::CheckOverallSize(LPPOINT diff, BOOL adjust) {

	CStatic *messageItem = (CStatic *) GetDlgItem(IDC_STATIC1);
	CButton *checkItem = (CButton *) GetDlgItem(IDC_CHECK1),
			*okButton = (CButton *) GetDlgItem(IDOK),
			*cancelButton = (CButton *) GetDlgItem(IDCANCEL);
	RECT	wRect,
			parentRect;
	POINT	border;
	HWND	parent = m_pParentWnd ? m_pParentWnd->GetSafeHwnd() : NULL;

	// calculate current minimum border. assumes buttons fix the bottom margin and
	// other items fix the right margin
	messageItem->GetWindowRect(&wRect);
	border.x = wRect.right;
	checkItem->GetWindowRect(&wRect);
	if (wRect.right > border.x)
		border.x = wRect.right;
	cancelButton->GetWindowRect(&wRect);
	border.y = wRect.bottom;
	if (wRect.right > border.x)
		border.x = wRect.right;
	GetWindowRect(&wRect);

	if (adjust) {
		// since moving the window seems to make the system no longer center it,
		// we have to do it ourselves
		if (m_pParentWnd)
			m_pParentWnd->GetWindowRect(&parentRect);
		else
			GetDesktopWindow()->GetWindowRect(&parentRect);

		// adjust dialog window size to keep the same borders between it and its subwindows
		border.x = diff->x - (wRect.right - border.x);
		border.y = diff->y - (wRect.bottom - border.y);
		wRect.right += border.x;
		wRect.bottom += border.y;

		// center it in its parent
		border.x = ((parentRect.right + parentRect.left) - (wRect.right + wRect.left)) / 2;
		border.y = ((parentRect.bottom + parentRect.top) - (wRect.bottom + wRect.top)) / 2;
		wRect.left += border.x;
		wRect.right += border.x;
		wRect.top += border.y;
		wRect.bottom += border.y;
		MoveWindow(&wRect, TRUE);
	} else {
		diff->x = wRect.right - border.x;
		diff->y = wRect.bottom - border.y;
	}
}

/* special adjustment for the buttons.  we won't make them smaller, and we'll keep
   them centered.  we also assume they're in a row at the bottom, with OK on the left,
   and the same size. */
void CCheckConfirmDialog::AdjustButtons(CWnd *okButton, CWnd *cancelButton,
										LONG expectedMargin) {

	RECT	okRect,
			cancelRect,
			newOKRect,
			newCancelRect,
			dialogRect;
	POINT	diff,
			tempDiff;
	LONG	separation,
			width;

	// assume if OK has no text change, than cancel doesn't either. no adjustment, then.
	if (mOKMessage.IsEmpty())
		return;

	okButton->GetWindowRect(&okRect);
	cancelButton->GetWindowRect(&cancelRect);
	GetWindowRect(&dialogRect);

	// calculate appropriate size
	RectForText(okButton, (const char *) mOKMessage, &newOKRect, &diff);
	RectForText(cancelButton, (const char *) mCancelMessage, &newCancelRect, &tempDiff);
	if (newOKRect.right - newOKRect.left > newCancelRect.right - newCancelRect.left)
		width = newOKRect.right - newOKRect.left;
	else {
		width = newCancelRect.right - newCancelRect.left;
		diff.x = tempDiff.x;
	}

	// don't shrink the buttons; only expand them
	if (diff.x > 0) {
		separation = cancelRect.left - okRect.right;
		okRect.left -= diff.x;
		cancelRect.right += diff.x;
		if (okRect.left - dialogRect.left < expectedMargin) {
			okRect.left = dialogRect.left + expectedMargin;
			okRect.right = okRect.left + width;
		}
		if (cancelRect.left < okRect.right + separation) {
			cancelRect.left = okRect.right + separation;
			cancelRect.right = cancelRect.left + width;
		}
		::MapWindowPoints(HWND_DESKTOP, GetSafeHwnd(), (LPPOINT) &okRect, 2);
		::MapWindowPoints(HWND_DESKTOP, GetSafeHwnd(), (LPPOINT) &cancelRect, 2);
		okButton->MoveWindow(&okRect, TRUE);
		cancelButton->MoveWindow(&cancelRect, TRUE);
	}
}

/* CUserSelectionDialog: presents a scrolling list of items with an initial
   selection, allowing the user to specify a selection
*/
BEGIN_MESSAGE_MAP(CUserSelectionDialog, CDialog)
END_MESSAGE_MAP()

CUserSelectionDialog::CUserSelectionDialog(CWnd *pParent, const char *pMessage,
        const char **pUserList, int16 nUserListCount) :

	CDialog(CUserSelectionDialog::IDD, pParent),
	mMessage(pMessage) {

	int16	ctr;

	mSelection = -1;
	// copy pUserList
	mList = (char **) XP_ALLOC(nUserListCount*sizeof(char *));
	mListCount = nUserListCount;
	if (mList) {
		for (ctr = 0; ctr < nUserListCount; ctr++) {
			int len = 1 + XP_STRLEN(pUserList[ctr]);
			char *newStr = (char *) XP_ALLOC(len*sizeof(char));
			if (newStr) {
				XP_STRCPY(newStr, pUserList[ctr]);
				mList[ctr] = newStr;
			} else {
				mListCount = ctr;
				break;
			}
		}
		if (mListCount == 0) {
			XP_FREE(mList);
			mList = 0;
		}
	}
}

CUserSelectionDialog::~CUserSelectionDialog() {

	int16 ctr;

	if (mList) {
		for (ctr = 0; ctr < mListCount; ctr++)
			XP_FREE(mList[ctr]);
		XP_FREE(mList);
	}
}

BOOL CUserSelectionDialog::DoModal(int16 *nSelection)
{
	BOOL	rtnval = CDialog::DoModal() == IDOK;
	*nSelection = mSelection;
	return rtnval;
}

void CUserSelectionDialog::OnOK()
{
	CListBox *listItem = (CListBox *) GetDlgItem(IDC_LIST1);
	int selection = listItem->GetCurSel();
	mSelection = selection == LB_ERR ? -1 : (int16) selection;
	CDialog::OnOK();
}

BOOL CUserSelectionDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	CStatic *messageItem = (CStatic *) GetDlgItem(IDC_STATIC1);
	CListBox *listItem = (CListBox *) GetDlgItem(IDC_LIST1);
	int16 ctr;

	// set subwindows' text
	messageItem->SetWindowText((const char *) mMessage);
	if (mList)
		for (ctr = 0; ctr < mListCount; ctr++) {
			int err;
			err = listItem->AddString(mList[ctr]);
			if (err == LB_ERR || err == LB_ERRSPACE)
				break;
		}

	return TRUE;
}

/* CSelfAdjustingDialog: some base code for a dialog that can adjust its size
   and its subwindows
 */

BEGIN_MESSAGE_MAP(CSelfAdjustingDialog, CDialog)
END_MESSAGE_MAP()

CSelfAdjustingDialog::CSelfAdjustingDialog(UINT nIDTemplate, CWnd* pParent) :
	CDialog(nIDTemplate, pParent) {
}

/* calculate appropriate window rect for its text. assumes average character width.
   returns actual (desktop-relative) window rect, adjusted for appropriate size,
   (without actually changing the window's rect). */
void CSelfAdjustingDialog::RectForText(CWnd *window, const char *text,
									   LPRECT wrect, LPPOINT diff) {

	int			height,
				width;
	const char	*mark;
	char		thisChar;
	BOOL		lastWasCR;
	CDC			*dc = window->GetDC();
	CSize		basicExtent = dc->GetTextExtent("W",1),
				extent;

	window->GetWindowRect(wrect);

	// calculate width and height of text.
	height = 0;
	width = 0;
	mark = text;
	lastWasCR = FALSE;
	while (1) {
		thisChar = *text;
		if (lastWasCR && thisChar == '\n') {
			lastWasCR = FALSE;
			mark = ++text;
			continue;
		}
		if (thisChar == '\r' || thisChar == '\n' || thisChar == '\0') {
			if (text-mark == 0) // it's a zero-length line
				height += basicExtent.cy;
			else {
				extent = dc->GetTextExtent(mark, text-mark);
				if (width < extent.cx)
					width = extent.cx;
				height += extent.cy;
			}
			mark = text+1;
			if (*text == '\0')
				break;
		}
		lastWasCR = *text++ == '\r';
	}
	if (width == 0)
		width = basicExtent.cx;
	if (height == 0)
		height = basicExtent.cy;
	diff->x = width - (wrect->right - wrect->left);
	diff->y = height - (wrect->bottom - wrect->top);
	wrect->right = wrect->left + width;
	wrect->bottom = wrect->top + height;
}

/* resize subwindow to fit its text. assumes average character width */
void CSelfAdjustingDialog::ResizeItemToFitText(CWnd *window, const char *text, LPPOINT diff) {

	RECT	windRect;

	RectForText(window, text, &windRect, diff);
	::MapWindowPoints(HWND_DESKTOP, GetSafeHwnd(), (LPPOINT) &windRect, 2);
	window->MoveWindow(&windRect, TRUE);
}

/* adjust window size for a change in an item size, and then adjust positions of
   affected subwindows */
void CCheckConfirmDialog::AdjustForItemSize(CWnd *afterWind, LPPOINT diff) {

	CStatic *messageItem = (CStatic *) GetDlgItem(IDC_STATIC1);
	CButton *checkItem = (CButton *) GetDlgItem(IDC_CHECK1),
			*okButton = (CButton *) GetDlgItem(IDOK),
			*cancelButton = (CButton *) GetDlgItem(IDCANCEL);
	HWND	parent = m_pParentWnd ? m_pParentWnd->GetSafeHwnd() : NULL;

	// adjust positions of trailing subwindows
	BumpItemIfAfter(messageItem, afterWind, diff);
	BumpItemIfAfter(checkItem, afterWind, diff);
	BumpItemIfAfter(okButton, afterWind, diff);
	BumpItemIfAfter(cancelButton, afterWind, diff);
}

/* adjust subwindows affected by a change in size of another subwindow */
void CSelfAdjustingDialog::BumpItemIfAfter(CWnd *item, CWnd *afterWind, LPPOINT diff) {

	RECT	afterRect,
			itemRect;

	item->GetWindowRect(&itemRect);
	afterWind->GetWindowRect(&afterRect);
	if (	diff->x != 0 &&
			itemRect.left > afterRect.left &&
			itemRect.top < afterRect.bottom && itemRect.bottom >= afterRect.top) {
		::MapWindowPoints(HWND_DESKTOP, GetSafeHwnd(), (LPPOINT) &itemRect, 2);
		itemRect.left += diff->x;
		itemRect.right += diff->x;
		item->MoveWindow(&itemRect, TRUE);
	}
	else if (	diff->y != 0 &&
				itemRect.top > afterRect.top &&
				itemRect.left < afterRect.right && itemRect.right >= afterRect.left) {
		::MapWindowPoints(HWND_DESKTOP, GetSafeHwnd(), (LPPOINT) &itemRect, 2);
		itemRect.top += diff->y;
		itemRect.bottom += diff->y;
		item->MoveWindow(&itemRect, TRUE);
	}
}
