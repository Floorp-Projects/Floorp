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

// eddialog.cpp : Dialogs used by the CNetscapeEditView class
//
//
// Created 10/11/96 by CLM
//
#include "stdafx.h"
#include "edview.h"
#include "edt.h"
#include "secnav.h"
#include "prefapi.h"
#include "nethelp.h"
#include "xp_help.h"
// the dialog box & string resources are in edtrcdll DLL
#include "edtrcdll\src\resource.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern char *EDT_NEW_DOC_NAME;

/////////////////////////////////////////////////////////////////////////////
// CLoadingImageDlg dialog
// 
CLoadingImageDlg::CLoadingImageDlg(CWnd* pParent, MWContext * pMWContext)
	: CDialog(CLoadingImageDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLoadingImageDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
    ASSERT(pParent);
    ASSERT(pMWContext);

    m_pMWContext = pMWContext;

    if (!CDialog::Create(CLoadingImageDlg::IDD, pParent))
    {
        TRACE0("Warning: creation of CLoadingImageDlg dialog failed\n");
        return;
    }

    // Center the window relative to parent    
    CRect cRectParent;
    CRect cRectDlg;
    pParent->GetWindowRect(&cRectParent);
    GetWindowRect(&cRectDlg);
    int iTop = 200;
    int iLeft = 200;
    if ( cRectParent.Height() > cRectDlg.Height() ){
        iTop = cRectParent.top + ( (cRectParent.Height() - cRectDlg.Height()) / 2 );
    }
    if ( cRectParent.Width() > cRectDlg.Width() ){
        iLeft = cRectParent.left + ( (cRectParent.Width() - cRectDlg.Width()) / 2 );
    }
    
    SetWindowPos( &wndTopMost, iLeft, iTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();
}


void CLoadingImageDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLoadingImageDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLoadingImageDlg, CDialog)
	//{{AFX_MSG_MAP(CLoadingImageDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLoadingImageDlg message handlers
void CLoadingImageDlg::OnCancel() 
{
    // Tell edit core to end loading
    // Note: This will result in call to FE_ImageLoadDialogDestroy,
    //       which will do DestroyWindow();
    EDT_ImageLoadCancel(m_pMWContext);
}

void CLoadingImageDlg::PostNcDestroy() 
{
	CDialog::PostNcDestroy();
    delete this;
}

/////////////////////////////////////////////////////////////////////
CSaveNewDlg::CSaveNewDlg(CWnd * pParent)
	: CDialog(CSaveNewDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSaveNewDlg)
	//}}AFX_DATA_INIT
}


BOOL CSaveNewDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

	CDialog::OnInitDialog();
    
	CStatic * wndIcon = (CStatic *)GetDlgItem(IDC_INFO_ICON);
    wndIcon->SetIcon(theApp.LoadStandardIcon(IDI_ASTERISK));
    
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(CSaveNewDlg, CDialog)
	//{{AFX_MSG_MAP(CSaveNewDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSaveFileDlg dialog
//
CSaveFileDlg::CSaveFileDlg(CWnd* pParent,
                           MWContext * pMWContext,
                           int  iFileCount,
                           ED_SaveDialogType saveType )
	: CDialog(CSaveFileDlg::IDD, pParent),
      m_pParent(pParent),
      m_hTimer(0)
{
    ASSERT(pMWContext);
    m_pMWContext = pMWContext;
    m_iFileCount = iFileCount;
    m_iCurrentFile = 0;
    m_bUpload = saveType == ED_SAVE_DLG_PUBLISH;
    
    // For simplicity, create right here
    if (!CDialog::Create(CSaveFileDlg::IDD, pParent))
    {
        TRACE0("Warning: creation of CSaveFileDlg dialog failed\n");
        return;
    }

        // Change dialog caption and message to use for uploading;
    UINT nID = 0;
    BOOL bPublishing = TRUE;
    UINT nIDCaption = IDS_PUBLISHING_DOCUMENT;
    switch ( saveType ){
        case ED_SAVE_DLG_SAVE_LOCAL:
            nID = IDS_SAVING_FILE;
            bPublishing = FALSE; // Use Caption in dialog resource
            break;
        case ED_SAVE_DLG_PREPARE_PUBLISH:
            nID = IDS_PREPARING_PUBLISH;
            break;
        case ED_SAVE_DLG_PUBLISH:
            nID = IDS_UPLOADING_FILE;
            break;
    }
    if( bPublishing ){
        SetWindowText(szLoadString(IDS_PUBLISHING_DOCUMENT));
    }
    if( nID ){
        GetDlgItem(IDC_SAVE_FILE_LABEL)->SetWindowText(szLoadString(nID));
    }

    // Clear our place-holder string
    GetDlgItem(IDC_FILE_COUNT_MSG)->SetWindowText("");

    // Start a short timer -- we don't show window until timer is hit
    // This prevents ugly flashing of this dialog when saving proceeds 
    //   real quick on local drive
    m_hTimer = SetTimer(1, 200, NULL);

    // Why doesn't this center on parent window automatically!
    CRect cRectParent;
    CRect cRectDlg;
    GetWindowRect(&cRectDlg);
    // Center in screen
    int iLeft;
    int iTop;
    UINT nStyle = SWP_NOSIZE;

    if( pParent ){
        pParent->GetWindowRect(&cRectParent);
        if ( cRectParent.Height() > cRectDlg.Height() ){
            iTop = cRectParent.top + ( (cRectParent.Height() - cRectDlg.Height()) / 2 );
        } else {
            iTop = cRectParent.top + GetSystemMetrics(SM_CYCAPTION);
        }
        if ( cRectParent.Width() > cRectDlg.Width() ){
            iLeft = cRectParent.left + ( (cRectParent.Width() - cRectDlg.Width()) / 2 );
        } else {
            iLeft = cRectParent.left;
        }
    } else {
        iLeft = (sysInfo.m_iScreenWidth / 2) - (cRectDlg.Width()/2);
        iTop  = (sysInfo.m_iScreenHeight / 2) - (cRectDlg.Height()/2);
    }

    if( m_hTimer ){
        // Hide window until timer fires or File system closes us
        nStyle |= SWP_HIDEWINDOW;
    } else {
        // No timer - we better show the dialog now
        nStyle |= SWP_SHOWWINDOW;
    }
    // Don't use &wndTopmost because CSaveFileOverwriteDlg
    //  will be popping up over us.
    SetWindowPos(NULL, iLeft, iTop, 0, 0, nStyle);
}

void CSaveFileDlg::OnTimer(UINT nIDEvent)
{
    ShowWindow(SW_SHOW);
    KillTimer(1);
    m_hTimer = 0;
    TRACE("CSaveFileDlg::OnTimer - Show window now\n");
}

void CSaveFileDlg::StartFileSave(char * pFilename)
{
    GetDlgItem(IDC_SAVE_FILENAME)->SetWindowText(pFilename);
    // Report file number if we have more than 1 total
    if( m_iFileCount > 1 ){
        m_iCurrentFile++;
        CString csFileCount;
        csFileCount.Format(szLoadString(IDS_FILE_COUNT_FORMAT), m_iCurrentFile, m_iFileCount );
        GetDlgItem(IDC_FILE_COUNT_MSG)->SetWindowText(csFileCount);
    }
}

void CSaveFileDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSaveFileDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSaveFileDlg, CDialog)
	//{{AFX_MSG_MAP(CSaveFileDlg)
    ON_WM_TIMER()
    ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSaveFileDlg message handlers

void CSaveFileDlg::OnDestroy() 
{
    if( m_hTimer ){
        KillTimer(1); 
    }
}

void CSaveFileDlg::OnCancel() 
{
    if( m_bUpload ){
        // Get a window context from MWContext
        CWinCX *pContext = VOID2CX(m_pMWContext->fe.cx, CWinCX);
        if( pContext ){
            pContext->Interrupt();
        }
        // NET_SilentInterruptWindow(m_pMWContext);
    } else {
        EDT_SaveCancel(m_pMWContext);
    }
    // Do NOT call CDialog::OnCancel(),
    //  Edit core will call to destroy us
}

void CSaveFileDlg::PostNcDestroy() 
{
	CDialog::PostNcDestroy();
}

/////////////////////////////////////////////////////////////////////////////
// CSaveFileOverwriteDlg dialog


CSaveFileOverwriteDlg::CSaveFileOverwriteDlg(CWnd* pParent /*=NULL*/,
                                             char * pFilename,
                                             CSaveFileDlg * pSaveDlg)
            : CDialog(CSaveFileOverwriteDlg::IDD, pParent )
{
	//{{AFX_DATA_INIT(CSaveFileOverwriteDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
    m_pFilename = pFilename;
    m_pSaveFileDlg = pSaveDlg;
}


BOOL CSaveFileOverwriteDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

	CDialog::OnInitDialog();
    CFont * pfontBold = CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FONT));
    CWnd * pWnd = GetDlgItem(IDC_SAVE_FILENAME);
    if ( pWnd ) {
        pWnd->SetFont(pfontBold);
        if ( m_pFilename ) {
            char *pName = XP_STRRCHR(m_pFilename, '\\');
            if( pName ){
                pName++;
            } else {
                pName = m_pFilename;
            }
            pWnd->SetWindowText(pName);
        }
    }

    // Set the question icon
	CStatic * wndIcon = (CStatic *)GetDlgItem(IDC_OVERWRITE_ICON);
    wndIcon->SetIcon(theApp.LoadStandardIcon(IDI_EXCLAMATION)); // sIDI_QUESTION));
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSaveFileOverwriteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSaveFileOverwriteDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSaveFileOverwriteDlg, CDialog)
	//{{AFX_MSG_MAP(CSaveFileOverwriteDlg)
	ON_BN_CLICKED(IDC_DONT_OVERWRITE_ALL, OnDontOverwriteAll)
	ON_BN_CLICKED(IDC_DONT_OVERWRITE_ONE, OnDontOverwriteOne)
	ON_BN_CLICKED(IDC_OVERWRITE_ALL, OnOverwriteAll)
	ON_BN_CLICKED(IDC_OVERWRITE_ONE, OnOverwriteOne)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSaveFileOverwriteDlg message handlers

int CSaveFileOverwriteDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

    // Offset dialog to upper left of parent
    //  Gives a cascade look relative to CSaveFileDlg
    if ( m_pSaveFileDlg ) {
        RECT rectParent;
        m_pSaveFileDlg->GetClientRect(&rectParent);
        m_pSaveFileDlg->ClientToScreen(&rectParent);
        SetWindowPos( 0, /*&wndTopMost,*/ 
                      rectParent.left, rectParent.top,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

	return 0;
}

void CSaveFileOverwriteDlg::OnOK()
{
    CDialog::OnOK();
}

void CSaveFileOverwriteDlg::OnDontOverwriteAll() 
{
    m_Result = ED_SAVE_DONT_OVERWRITE_ALL;
    OnOK();
}

void CSaveFileOverwriteDlg::OnDontOverwriteOne() 
{
    m_Result = ED_SAVE_DONT_OVERWRITE_THIS;
    OnOK();
}

void CSaveFileOverwriteDlg::OnOverwriteAll() 
{
    m_Result = ED_SAVE_OVERWRITE_ALL;
    OnOK();
}

void CSaveFileOverwriteDlg::OnOverwriteOne() 
{
    
    m_Result = ED_SAVE_OVERWRITE_THIS;
    OnOK();
}

void CSaveFileOverwriteDlg::OnCancel() 
{
    m_Result = ED_SAVE_CANCEL;
    OnOK();
}


/////////////////////////////////////////////////////////////////////////////
// Local helpers for publish string manipulation

static void FreeStrings(char ** pStrings)
{
    if(pStrings){
        int i = 0;
        while( pStrings[i] ){
            XP_FREE(pStrings[i]);
            pStrings[i] = NULL;
            i++;
        }
        XP_FREE(pStrings);
    }
}

/////////////////////////////////////////////////////////////////////////////
// CPublishDlg dialog


CPublishDlg::CPublishDlg(CWnd *pParent,
                         MWContext *pMWContext,
                         char *pUrl)
	: CDialog(CPublishDlg::IDD, pParent),
    m_pMWContext(pMWContext),
    m_pCurrentUrl(pUrl),
    m_pCurrentFile(0),
    m_pCurrentDirectory(0),
    m_pImageFiles(0),
    m_pSelectedDefault(0),
    m_ppAllFiles(0),
    m_bIsRootDirectory(0),
    m_ppUserList(0),
    m_ppPasswordList(0),
    m_pFullLocation(0),
    m_ppImageList(0),
    m_iFileCount(0)
{
  // m_pCurrentFile may be NULL, but m_pCurrentUrl won't be.
  // m_pCurrentFile will be non-NULL iff we are publishing a local file.
  //
  // if (pUrl != file:///Untitled)
  if (XP_STRCMP(pUrl,EDT_NEW_DOC_NAME)) {
    XP_ConvertUrlToLocalFile( pUrl, &m_pCurrentFile );
  }

	//{{AFX_DATA_INIT(CPublishDlg)
	m_csUserName = _T("");
	m_csPassword = _T("");
	m_csTitle = _T("");
	m_csFilename = _T("");
	m_bRememberPassword = TRUE;
	//}}AFX_DATA_INIT
    ASSERT( pMWContext );
}

CPublishDlg::~CPublishDlg()
{
    XP_FREEIF(m_pCurrentFile);
    XP_FREEIF(m_pCurrentDirectory);
    XP_FREEIF(m_pImageFiles);
    XP_FREEIF(m_pSelectedDefault);
    XP_FREEIF(m_pFullLocation);
    FreeStrings(m_ppImageList);
    FreeStrings(m_ppAllFiles);
    FreeStrings(m_ppUserList);
    FreeStrings(m_ppPasswordList);
    //NOTE: DO NOT try to free m_ppIncludedFiles list - EDT_PublishFile will do that

}

void CPublishDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPublishDlg)
	DDX_Text(pDX, IDC_PUBLISH_USER, m_csUserName);
	DDX_Text(pDX, IDC_PUBLISH_PASSWORD, m_csPassword);
	DDX_Text(pDX, IDC_DOC_TITLE, m_csTitle);
	DDX_Text(pDX, IDC_PUBLISH_FILENAME, m_csFilename);
	DDX_Check(pDX, IDC_PUBLISH_REMEMBER_PASSWORD, m_bRememberPassword);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPublishDlg, CDialog)
	//{{AFX_MSG_MAP(CPublishDlg)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_SELECT_ALL, OnSelectAll)
	ON_BN_CLICKED(IDC_SELECT_NONE, OnSelectNone)
	ON_BN_CLICKED(IDC_INCLUDE_ALL_FILES, OnIncludeAllFiles)
	ON_BN_CLICKED(IDC_INCLUDE_IMAGE_FILES, OnIncludeImageFiles)
	ON_CBN_KILLFOCUS(IDC_PUBLISH_LOCATION_LIST, OnKillfocusPublishLocationList)
	ON_BN_CLICKED(IDC_PUBLISH_DEFAULT_LOCATION, OnPublishDefaultLocation)
	ON_CBN_SELCHANGE(IDC_PUBLISH_LOCATION_LIST, OnSelchangePublishLocation)
	//}}AFX_MSG_MAP
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()

BOOL CPublishDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();

    // TODO: Compress long url name, so fits in list box.
    CString csURL = m_pCurrentFile ? m_pCurrentFile : m_pCurrentUrl;
    WFE_CondenseURL(csURL, 50, FALSE);
    
    CString csCaption;
    csCaption.Format(szLoadString(IDS_PUBLISH_CAPTION), csURL);
    // Display the main filepath (or URL) in dialog Caption
    SetWindowText(csCaption);

    CComboBox *pLocationComboBox;
    VERIFY(pLocationComboBox = (CComboBox*)GetDlgItem(IDC_PUBLISH_LOCATION_LIST));

    XP_Bool bRem;
    PREF_GetBoolPref("editor.publish_save_password",&bRem);
    m_bRememberPassword = bRem;

    char * pPassword = NULL;
    char * pFilename = NULL;
    char * pLastLoc = NULL;
    char * pInitLocation = NULL;
    char * pInitUserName = NULL;

    // Get the default publish URL and password
    pInitLocation = EDT_GetDefaultPublishURL(m_pMWContext, &pFilename, &pInitUserName, &pPassword);
    if( pFilename ){
        m_csFilename = pFilename;
        // Don't free yet - we may use it for title
    }

    if( pPassword ){
        m_csPassword = pPassword;
        XP_FREEIF(pPassword);
    }

    // It would be nice to use CString::GetBuffer, but we can't XP_FREE its contents!
    m_csLocation = pInitLocation;
    m_csUserName = pInitUserName;
    
    // Don't use DDX for this
    pLocationComboBox->SetWindowText(pInitLocation);

    XP_FREEIF(m_pFullLocation);
    XP_FREEIF(pInitUserName);

    EDT_PageData * pPageData = EDT_GetPageData(m_pMWContext);
    if( pPageData ){
        m_csTitle = pPageData->pTitle;
        EDT_FreePageData(pPageData);
    }

    // If empty, fill in title with Filename without extension
    if( m_csTitle.IsEmpty() && pFilename ){
        char * pTitle = EDT_GetPageTitleFromFilename(pFilename);
        if( pTitle ){
            m_csTitle = pTitle;
            XP_FREE(pTitle);
        }
    }
    XP_FREEIF(pFilename);
    
    // Enable the "Default location" button only if we have a preference
	char *prefStr = NULL;
	PREF_CopyCharPref("editor.publish_location", &prefStr);
	if (prefStr) XP_FREE(prefStr);

    // Build lists of 10 most-recently-visited sites saved in preferences
    // ComboBox will contain locations,
    //  we store username and password in local lists, same order (no sorting)
    VERIFY(m_ppUserList = (char**) XP_ALLOC((MAX_PUBLISH_LOCATIONS+1) * sizeof(char*)));
    VERIFY(m_ppPasswordList = (char**) XP_ALLOC((MAX_PUBLISH_LOCATIONS+1) * sizeof(char*)));
            
    // Fill the lists of user names and passwords that maps
    //  to the preference list. First item = most recently-added
    int i;
    BOOL bLocationFound = FALSE;
    for( i= 0; i < MAX_PUBLISH_LOCATIONS; i++ ){
        m_ppUserList[i] = NULL;
        m_ppPasswordList[i] = NULL;
        char *pLocation = NULL;
		if( EDT_GetPublishingHistory( i, &pLocation, &m_ppUserList[i], &m_ppPasswordList[i] ) &&
            pLocation && *pLocation ){
            pLocationComboBox->AddString(pLocation);
            // If previous location = the initial new location,
            //  then use the previous username and password 
            if( !bLocationFound && !strcmp(pLocation, pInitLocation) ){
                // Should there ever be an existing initial username? (not likely!)
                //if( m_csUserName.IsEmpty() ){
                m_csUserName = m_ppUserList[i]; 
                m_csPassword = m_ppPasswordList[i];
                // Don't check again once we found the most recent entry
                bLocationFound = TRUE;
            }
        } else {
            break;
        }
        XP_FREEIF(pLocation);
    } 

    // Terminate the lists
    m_ppUserList[i] = NULL;
    m_ppPasswordList[i] = NULL;

    XP_FREEIF(pInitLocation);

    // Copy the directory part of our source file
    // Weird: XP_OPENDIR needs a slash at beginning!
    if (m_pCurrentFile) {
        m_pCurrentDirectory = (char*)XP_ALLOC(XP_STRLEN(m_pCurrentFile)+2);
    }
    // Space for "/."
    else {
        m_pCurrentDirectory = (char*)XP_ALLOC(3);
    }

    if( !m_pCurrentDirectory ){
        // No memory!
        TRACE0("Not enough memory for publish string creation.\n");
        EndDialog(IDCANCEL);
        return TRUE;
    }
    XP_STRCPY(m_pCurrentDirectory,"/");  // So trailing \0 if m_pCurrentFile is NULL.
    // *m_pCurrentDirectory = '/';
    if (m_pCurrentFile) {
      XP_STRCPY((m_pCurrentDirectory+1),m_pCurrentFile);
    }

    // Find last slash '\' 
    char *pSlash =  strrchr(m_pCurrentDirectory, '\\'); 
    if( pSlash ){
        // Keep terminal slash if we are a root directory
        if( *(pSlash-1) == ':' ){
            m_bIsRootDirectory = TRUE;
            pSlash++;
        }
        *pSlash = '\0';
    } else {
        // No directory? Assume current?
        XP_STRCPY((m_pCurrentDirectory+1),".");
    }

    // Fill the list with image files contained in this document
    XP_FREEIF(m_pSelectedDefault);

	XP_Bool bKeepImages;
	PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
    m_pImageFiles = EDT_GetAllDocumentFilesSelected( m_pMWContext, &m_pSelectedDefault, bKeepImages );
    if( m_pImageFiles ){
        // Start with just the list of images in current file
        OnIncludeImageFiles();
    } else {
        // No images, disable buttons
        GetDlgItem(IDC_INCLUDE_IMAGE_FILES)->EnableWindow(FALSE);    
        GetDlgItem(IDC_SELECT_ALL)->EnableWindow(FALSE);
        GetDlgItem(IDC_SELECT_NONE)->EnableWindow(FALSE);
    }
    if( !m_pCurrentFile ) {
        GetDlgItem(IDC_INCLUDE_ALL_FILES)->EnableWindow(FALSE);    
    }

    ((CButton*)GetDlgItem(IDC_INCLUDE_IMAGE_FILES))->SetCheck(m_pImageFiles ? 1 : 0);
 
 	CDialog::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPublishDlg::OnOK() 
{
    UpdateData(TRUE);

    // Extract any username:password@ and filename from location editbox
    //  into m_csUserName, m_csPassword, and m_csFilename
    OnKillfocusPublishLocationList();
        
    m_csFilename.TrimLeft();
    m_csFilename.TrimRight();
    m_csTitle.TrimLeft();
    m_csTitle.TrimRight();


    // Prompt for a filename
    if( m_csFilename.IsEmpty() &&
        IDNO == MessageBox( szLoadString(IDS_ENTER_PUBLISH_FILENAME),
                            szLoadString(IDS_PUBLISH_PAGE),
                            MB_ICONEXCLAMATION | MB_YESNO) ){
        GetDlgItem(IDC_PUBLISH_FILENAME)->SetFocus();
        return;
    }

    if( m_csTitle.IsEmpty() ){
        char * pSuggested = EDT_GetPageTitleFromFilename((char*)LPCSTR(m_csFilename));        
        
        // Use same dialog we use after SaveAs to prompt user for a title
        CPageTitleDlg dlg(this, &pSuggested);
        if( dlg.DoModal() == IDOK ){
            m_csTitle = pSuggested;
        } else {
            // User canceled. Put them back in Publish dialog
            GetDlgItem(IDC_DOC_TITLE)->SetFocus();
            XP_FREEIF(pSuggested);
            return;
        }
        XP_FREEIF(pSuggested);
    }

    EDT_PageData * pPageData = EDT_GetPageData(m_pMWContext);
    if( pPageData ){
      char *pNewTitle;
      if( m_csTitle.IsEmpty() ){
          pNewTitle = NULL;
      } else {    
          pNewTitle = XP_STRDUP(LPCSTR(m_csTitle));
      }

      // Only set if value has changed.  O.W. Messes up the editor dirty flag.
      if (XP_STRCMP(pPageData->pTitle ? pPageData->pTitle : "",
                    pNewTitle ? pNewTitle : "")) {
        XP_FREEIF(pPageData->pTitle);
        pPageData->pTitle = pNewTitle; 
        EDT_SetPageData(m_pMWContext, pPageData);
      }
      else {
        XP_FREEIF(pNewTitle);
      }
  
      EDT_FreePageData(pPageData);
    }

    ((CComboBox*)GetDlgItem(IDC_PUBLISH_LOCATION_LIST))->GetWindowText(m_csLocation);
    m_csLocation.TrimLeft();
    m_csLocation.TrimRight();
    
    int type = NET_URL_Type((char*)LPCSTR(m_csLocation));
    if( type == FTP_TYPE_URL  ||
        type == HTTP_TYPE_URL ||
        type == SECURE_HTTP_TYPE_URL ){

        m_csUserName.TrimLeft();
        m_csUserName.TrimRight();
        m_csPassword.TrimLeft();
        m_csPassword.TrimRight();
        m_csLocation.TrimLeft();
        m_csLocation.TrimRight();

        // Assemble a string WITH password to do actual upload
        char * pLocation = NULL;
        NET_MakeUploadURL( &pLocation, (char*)LPCSTR(m_csLocation), 
                           (char*)LPCSTR(m_csUserName),
                           (char*)LPCSTR(m_csPassword) );
              
        // Add filename to end of location URL for validation
        // This is final string used by Publishing
        m_pFullLocation = EDT_ReplaceFilename(pLocation, (char*)LPCSTR(m_csFilename), TRUE);
        XP_FREEIF(pLocation);
        
        // HARDTS:        
        // Tell user the URL they are publishing looks like it might be wrong.
        // e.g. ends in a slash or does not have a file extension.
        // Give the user the option of attempting to publish to the
        // specified URL even if it looks suspicious.
        if (!EDT_CheckPublishURL(m_pMWContext,m_pFullLocation)) {
            return;
        }

        CListBox * pIncludeListBox = (CListBox*)GetDlgItem(IDC_PUBLISH_OTHER_FILES);
        int iCount = pIncludeListBox->GetSelCount();
        
        // Plus one extra string to terminate array
    	m_ppIncludedFiles = (char**) XP_ALLOC((iCount+1) * sizeof(char*));

        if( !m_ppIncludedFiles ){
            // Unlikely! Not enough memory!
            EndDialog(IDCANCEL);
            return;
        }
        BOOL bUseImageList = ((CButton*)GetDlgItem(IDC_INCLUDE_IMAGE_FILES))->GetCheck();

        // Construct an array of included files
        //   from just the selected items in listbox
        if( iCount ){
            int *pIndexes = (int*)XP_ALLOC(iCount * sizeof(int));
            if( pIndexes ){
                pIncludeListBox->GetSelItems(iCount, pIndexes);
                CString csItem;
                for( int i=0; i < iCount; i++ ){
                    if( bUseImageList ){
                        // Copy the URL from the original, sorted list of image URLs
                        csItem = m_ppImageList[pIndexes[i]];
                    } else {
                        pIncludeListBox->GetText(pIndexes[i], csItem);
                    }
                    m_ppIncludedFiles[i] = XP_STRDUP(LPCSTR(csItem));
                }
                XP_FREE(pIndexes);
            }
        }
        // Terminate array
        m_ppIncludedFiles[iCount] = NULL;

        // Don't use CDialog::OnOK() -- it will overwrite our changed m_csLocation
        EndDialog(IDOK);
    } else {
        // Tell user they must use "ftp://" or "http://"
        MessageBox(szLoadString(IDS_BAD_PUBLISH_URL),
                   szLoadString(IDS_PUBLISH_FILES),
                   MB_ICONEXCLAMATION | MB_OK);
    }
}

void CPublishDlg::OnHelp() 
{
    NetHelp(HELP_PUBLISH_FILES);
}



#ifdef XP_WIN32
BOOL CPublishDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32



void CPublishDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

// Implemented in edprops.cpp
extern int CompareStrings( const void *arg1, const void *arg2 );

// Maximum number of characters of a URL in the Included Files listbox
#define ED_MAX_URL_LENGTH 55

void CPublishDlg::OnIncludeImageFiles() 
{
    if( m_pImageFiles ){
        FreeStrings(m_ppImageList);
        m_iFileCount = 0;

        char * pString = m_pImageFiles;
        int iLen;    
        // Scan list once to count items
        while( (iLen = XP_STRLEN(pString)) > 0 ) {
            pString += iLen+1;
            m_iFileCount++;
        }
        m_ppImageList = (char**)XP_ALLOC((m_iFileCount+1) * sizeof(char*));
        if(!m_ppImageList){
            return;
        }

        int i = 0;
        pString = m_pImageFiles;

        for( i = 0; i< m_iFileCount; i++){
            iLen = XP_STRLEN(pString) + 1;
            // Allocate room for string + 1 char at end for selected signal
            //   (cause index list we have will be useless once names are sorted)
            char * pItem = (char*)XP_ALLOC((iLen+1) * sizeof(char));
            if( pItem ){
                strcpy(pItem,pString);
                pItem[iLen] = m_pSelectedDefault[i] ? ' ' : '\0';
                m_ppImageList[i] = pItem;
            } else {
                FreeStrings(m_ppImageList);
                m_ppImageList = NULL;
                m_iFileCount = 0;
                return;
            }
            pString += iLen;
        }
        // Terminate the list
        m_ppImageList[i] = NULL;

        // Sort the strings
        qsort( (void *)m_ppImageList, (size_t)m_iFileCount, sizeof( char * ), CompareStrings );

        CListBox * pIncludeListBox = (CListBox*)GetDlgItem(IDC_PUBLISH_OTHER_FILES);
        pIncludeListBox->ResetContent();
        for(i = 0; i < m_iFileCount; i++){
            // add to the end of the list
            pString = m_ppImageList[i];
            iLen = strlen(pString);
            if( iLen > ED_MAX_URL_LENGTH ){
                CString csItem = pString;
                WFE_CondenseURL(csItem, ED_MAX_URL_LENGTH, FALSE);
                pIncludeListBox->AddString(LPCSTR(csItem));
            } else {
                pIncludeListBox->AddString(pString);
            }
            char SelChar = pString[strlen(pString)+1];
            // Last character past '\0' is " " for selected, or 0 if not selected by default
            pIncludeListBox->SetSel(i, SelChar ? 1 : 0);
        }

        GetDlgItem(IDC_SELECT_ALL)->EnableWindow(TRUE);
        GetDlgItem(IDC_SELECT_NONE)->EnableWindow(TRUE);
    }
}

void CPublishDlg::OnIncludeAllFiles() 
{
    // Shouldn't be here when editing a remote URL.
    if (!m_pCurrentFile) {
      ASSERT(0);
    }

    FreeStrings(m_ppAllFiles);

    // Always reread directory list in case use changes
    //   with another program
    m_ppAllFiles = NET_AssembleAllFilesInDirectory(m_pMWContext, 
                                                   m_pCurrentDirectory);

    // Get the count of items
    int i = 0;
    while( m_ppAllFiles[i] ){
        i++;
    }

    // Sort the strings
    qsort( (void *)m_ppAllFiles, (size_t)i, sizeof( char * ), CompareStrings );

    int iCount = 0;
    if( m_ppAllFiles && *m_ppAllFiles ){
        CListBox * pIncludeListBox = (CListBox*)GetDlgItem(IDC_PUBLISH_OTHER_FILES);
        pIncludeListBox->ResetContent();
        // Get current file without the directory
        char *pMainFile = m_pCurrentFile + XP_STRLEN(m_pCurrentFile);
        while( pMainFile != m_pCurrentFile && *pMainFile != '\\') pMainFile--;
        if( pMainFile != m_pCurrentFile ) pMainFile++;
        
        i = 0;
        char * pFilename;
        while( m_ppAllFiles[i] ){
            // Find just filename (skip over included directory)
            pFilename = strrchr(m_ppAllFiles[i], '\\');
            if( ! pFilename ){
                pFilename = strrchr(m_ppAllFiles[i], '/');
            }
            if( pFilename ){
                pFilename++;
            } else {
                pFilename = m_ppAllFiles[i];
            }
            // Don't include the main file in the list
            if( 0 != strcmpi(pFilename, pMainFile) ){
        		pIncludeListBox->AddString(pFilename);
                iCount++;
            }
            i++;
        }
        pIncludeListBox->SetSel(-1, TRUE);
    }
    // If no other files in the directory, clear button
    // (Note: we don't disable in case user copies files to 
    //  directory while dialog is still active)
    if( iCount == 0 ){
        ((CButton*)GetDlgItem(IDC_INCLUDE_ALL_FILES))->SetCheck(0);
    }
    GetDlgItem(IDC_SELECT_ALL)->EnableWindow(iCount > 0);
    GetDlgItem(IDC_SELECT_NONE)->EnableWindow(iCount > 0);
}

void CPublishDlg::OnSelectAll() 
{
    // Select all strings in the listbox
    ((CListBox*)GetDlgItem(IDC_PUBLISH_OTHER_FILES))->SetSel(-1, TRUE);
}

void CPublishDlg::OnSelectNone() 
{
    // Unselect all strings in the listbox
    ((CListBox*)GetDlgItem(IDC_PUBLISH_OTHER_FILES))->SetSel(-1, FALSE);
}

void CPublishDlg::OnSelchangePublishLocation() 
{
    // Get the corresponding username and passwords
    //  to show in edit boxes
    int i = ((CComboBox*)GetDlgItem(IDC_PUBLISH_LOCATION_LIST))->GetCurSel();

    if( i >= 0 && m_ppUserList && m_ppPasswordList ){
        // This is lame. We can't use UpdateData 
        //  or setting selected item doesn't work right!
        GetDlgItem(IDC_PUBLISH_USER)->SetWindowText(m_ppUserList[i]);
        GetDlgItem(IDC_PUBLISH_PASSWORD)->SetWindowText(m_ppPasswordList[i]);
    }
}

// Parse editfield location into URL, Filename, UserName, and Password
//  NOTE: Anything found in this Location will override contents of 
//        individual Filename, UserName, and Password edit boxes
void CPublishDlg::OnKillfocusPublishLocationList()
{
    // If user included a filename, user name, or password within location string,
    //  use that instead of current edit fields
    UpdateData(TRUE);

    char *pUserName = NULL;
    char *pPassword = NULL;
    char *pLocation = NULL;
    char *pFilename = NULL;
    GetDlgItem(IDC_PUBLISH_LOCATION_LIST)->GetWindowText(m_csLocation);
    m_csLocation.TrimLeft();
    m_csLocation.TrimRight();

    NET_ParseUploadURL( (char*)LPCSTR(m_csLocation), &pLocation, 
                        &pUserName, &pPassword );
    if( pUserName ){
        CString csUser = pUserName;
        csUser.TrimLeft();
        csUser.TrimRight();
        if( !csUser.IsEmpty() ){
            m_csUserName = csUser;
        }
        XP_FREE(pUserName);
    }
    if( pPassword ){
        CString csPassword = pPassword;
        csPassword.TrimLeft();
        csPassword.TrimRight();
        if( !csPassword.IsEmpty() ){
            m_csPassword = csPassword;
        }
        XP_FREE(pPassword);
    }
    if( pLocation ){
        // Extract filename at end of Location
        //  ONLY if it ends in ".htm" or ".html" or ".shtml"
        pFilename = EDT_GetFilename(pLocation, TRUE);
        // Put in check for pFilename to be NULL.  hardts
        char * pDot = pFilename ? strchr(pFilename, '.') : NULL;
        if( pFilename && *pFilename && pDot &&
            (0 == stricmp(".htm", pDot) || 0 == stricmp(".html", pDot) || 0 == stricmp(".shtml", pDot)) ){
            m_csFilename = pFilename;
            XP_FREE(pFilename);

            // Save version with filename stripped off
            char * pURL = EDT_ReplaceFilename(pLocation, NULL, TRUE);
            m_csLocation = pURL;
            GetDlgItem(IDC_PUBLISH_LOCATION_LIST)->SetWindowText(pURL);
            XP_FREEIF(pURL);
        } else {
            m_csLocation = pLocation;
        }
        XP_FREE(pLocation);
    }

    // Update Filename, User name, and password controls
    UpdateData(FALSE);
}


void CPublishDlg::OnPublishDefaultLocation() 
{
    UpdateData(TRUE);

    GetDlgItem(IDC_PUBLISH_LOCATION_LIST)->GetWindowText(m_csLocation);

    char * pDefaultLocation = NULL;
	PREF_CopyCharPref("editor.publish_location", &pDefaultLocation);
    if( !pDefaultLocation){
        return;
    }
    char *pLocation = NULL;
    char *pUserName = NULL;

    // Parse the preference string to extract Username
    //  password is separate for security munging
    NET_ParseUploadURL( pDefaultLocation, &pLocation, 
                        &pUserName, NULL );

    // It would be nice to use CString::GetBuffer, but we can't XP_FREE its contents!
    m_csLocation = pLocation;
    m_csUserName = pUserName;

	char * pPass = NULL;
	PREF_CopyCharPref("editor.publish_password", &pPass);
    char * pPassword = SECNAV_UnMungeString(pPass);
    m_csPassword = pPassword;
    XP_FREEIF(pPassword);

    GetDlgItem(IDC_PUBLISH_LOCATION_LIST)->SetWindowText(pLocation);
    
	XP_FREEIF(pDefaultLocation);
	XP_FREEIF(pLocation);
    XP_FREEIF(pUserName);
	XP_FREEIF(pPass);

    UpdateData(FALSE);
}

/////////////////////////////////////////////////////////////////////////////
// CEditHintDlg dialog


CEditHintDlg::CEditHintDlg(CWnd* pParent,
			   UINT  nID_Msg,
			   UINT  nID_Caption,
			   BOOL  bYesNo)     // Default style is MB_OK
	: CDialog(CEditHintDlg::IDD, pParent),
      m_nID_Msg(nID_Msg),
      m_nID_Caption(nID_Caption),
      m_bYesNo(bYesNo)
{
	//{{AFX_DATA_INIT(CEditHintDlg)
	m_bDontShowAgain = FALSE;
	m_cHintText = _T("");
	//}}AFX_DATA_INIT
}


void CEditHintDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditHintDlg)
	DDX_Check(pDX, IDC_SHOW_HINT_AGAIN, m_bDontShowAgain);
	DDX_Text(pDX, IDC_EDIT_HINT, m_cHintText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditHintDlg, CDialog)
	//{{AFX_MSG_MAP(CEditHintDlg)
	ON_BN_CLICKED(IDYES, OnYes)
	ON_BN_CLICKED(IDNO, OnNo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CEditHintDlg message handlers

BOOL CEditHintDlg::OnInitDialog() 
{
    m_ResourceSwitcher.Reset();
	
    CStatic * wndIcon = (CStatic *)GetDlgItem(IDC_INFO_ICON);
    wndIcon->SetIcon(theApp.LoadStandardIcon(IDI_ASTERISK));
    if( m_nID_Caption ){
        SetWindowText(szLoadString(m_nID_Caption));
    }
    if( m_nID_Msg ){
        m_cHintText.LoadString(m_nID_Msg);
    }
    if( m_bYesNo ){
        // Switch to Yes/No button pair
        GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
        GetDlgItem(IDYES)->ShowWindow(SW_SHOW);
        GetDlgItem(IDNO)->ShowWindow(SW_SHOW);
    }
	CDialog::OnInitDialog();
	return TRUE;  // return TRUE unless you set the focus to a control
		      // EXCEPTION: OCX Property Pages should return FALSE
}

void CEditHintDlg::OnOK() 
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}

void CEditHintDlg::OnYes() 
{
    OnOK();             
}

void CEditHintDlg::OnNo() 
{
    CDialog::OnCancel();
}

/////////////////////////////////////////////////////////////////////////////
CGetLocationDlg::CGetLocationDlg(CWnd* pParent,
			   UINT  nID_Msg,
			   UINT  nID_Caption,
               UINT  nID_FileCaption)
	: CDialog(CGetLocationDlg::IDD, pParent),
      m_nID_Msg(nID_Msg),
      m_nID_Caption(nID_Caption),
      m_nID_FileCaption(nID_FileCaption)
{
	//{{AFX_DATA_INIT(CGetLocationDlg)
	m_csLocation = _T("");
	//}}AFX_DATA_INIT
}

void CGetLocationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGetLocationDlg)
	DDX_Text(pDX, IDC_LOCATION, m_csLocation);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CGetLocationDlg, CDialog)
	//{{AFX_MSG_MAP(CGetLocationDlg)
	ON_BN_CLICKED(IDC_BROWSE_FILE, OnChooseFile)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGetLocationDlg message handlers

BOOL CGetLocationDlg::OnInitDialog() 
{
    m_ResourceSwitcher.Reset();
	
    if( m_nID_Caption ){
        SetWindowText(szLoadString(m_nID_Caption));
    }
    if( m_nID_Msg ){
        GetDlgItem(IDC_PREF_STRING_MSG)->SetWindowText(szLoadString(m_nID_Msg));
    }
	CDialog::OnInitDialog();
	return TRUE;
}

void CGetLocationDlg::OnOK() 
{
	CDialog::OnOK();
    m_csLocation.TrimLeft();
    m_csLocation.TrimRight();

    // In case user entered a local filename instead of using "Choose File"
    CString csLocation;
    WFE_ConvertFile2Url(csLocation, LPCSTR(m_csLocation));
    m_csLocation = csLocation;
}

void CGetLocationDlg::OnChooseFile() 
{
    char * szFilename = wfe_GetExistingImageFileName(this->m_hWnd, 
                                         szLoadString(m_nID_FileCaption), TRUE);
    if ( szFilename != NULL ){
        WFE_ConvertFile2Url(m_csLocation, szFilename);
        UpdateData(FALSE);
    }
}

/////////////////////////////////////////////////////////////////////////////
COpenTemplateDlg::COpenTemplateDlg(CWnd* pParent)
	: CDialog(COpenTemplateDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(COpenTemplateDlg)
	m_csLocation = _T("");
	//}}AFX_DATA_INIT
}

void COpenTemplateDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COpenTemplateDlg)
	DDX_Text(pDX, IDC_LOCATION, m_csLocation);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COpenTemplateDlg, CDialog)
	//{{AFX_MSG_MAP(COpenTemplateDlg)
	ON_BN_CLICKED(IDC_BROWSE_FILE, OnChooseFile)
    ON_BN_CLICKED(IDC_NETSCAPE_TEMPLATES, OnGotoNetscapeTemplates)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COpenTemplateDlg message handlers
BOOL COpenTemplateDlg::OnInitDialog() 
{
    m_ResourceSwitcher.Reset();
	
    CComboBox *pCombo = 
        (CComboBox*)(GetDlgItem(IDC_LOCATION));

    m_iTemplateLocationCount = theApp.m_iTemplateLocationCount;
    m_pHistoryBase = "editor.template_history_%d";
    //PREF_GetCharPref(,szLocation);

    if( m_iTemplateLocationCount ){
        // ComboBox will contain locations,
        //  we store username and password in local lists, same order (no sorting)
        VERIFY(m_pLocationList = (char**) XP_ALLOC((m_iTemplateLocationCount+1) * sizeof(char*)));
                
        // Fill the lists of user names and passwords that maps
        //  to the preference list. First item = most recently-added
        int i;
        for( i= 0; i < m_iTemplateLocationCount; i++ ){
            m_pLocationList[i] = NULL;
			char pLoc[128];
			char *pLocation = NULL;

			wsprintf(pLoc, m_pHistoryBase,i);
            // Get the preference value - RETURNS NULL IF EMPTY STRING!
            PREF_CopyCharPref(pLoc,&pLocation);
            if( pLocation && pLocation[0] ){
                pCombo->AddString(pLocation);
            }
        	XP_FREEIF(pLocation);
        } 
        // Terminate list
        m_pLocationList[i] = NULL;
    }
    CDialog::OnInitDialog();
    pCombo->SetCurSel(0);
	
	return TRUE;
}

void COpenTemplateDlg::OnOK() 
{
	CDialog::OnOK();
    m_csLocation.TrimLeft();
    m_csLocation.TrimRight();

    // In case user entered a local filename instead of using "Choose File"
    CString csLocation;
    WFE_ConvertFile2Url(csLocation, LPCSTR(m_csLocation));
    m_csLocation = csLocation;
}

void COpenTemplateDlg::OnChooseFile() 
{
    char * pFilename = wfe_GetExistingFileName(this->m_hWnd, 
                                               szLoadString(IDS_SELECT_TEMPLATE_CAPTION), HTM_ONLY, TRUE);
    if ( pFilename != NULL ){
        WFE_ConvertFile2Url(m_csLocation, pFilename);
        UpdateData(FALSE);
    }
}

void COpenTemplateDlg::OnGotoNetscapeTemplates()
{
    char * pDefault;
    PREF_CopyCharPref("editor.default_template_location", &pDefault);
    m_csLocation = pDefault;
    XP_FREEIF(pDefault);
    UpdateData(FALSE);
    EndDialog(IDOK);
}

/////////////////////////////////////////////////////////////////////////////
CPageTitleDlg::CPageTitleDlg(CWnd* pParent, char** ppTitle)
	: CDialog(CPageTitleDlg::IDD, pParent),
    m_ppTitle(ppTitle)
{
	//{{AFX_DATA_INIT(CPageTitleDlg)
	m_csTitle = _T("");
	//}}AFX_DATA_INIT
}

void CPageTitleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPageTitleDlg)
	DDX_Text(pDX, IDC_DOC_TITLE, m_csTitle);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPageTitleDlg, CDialog)
	//{{AFX_MSG_MAP(CPageTitleDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPageTitleDlg message handlers


BOOL CPageTitleDlg::OnInitDialog() 
{
    m_ResourceSwitcher.Reset();
    if( m_ppTitle && *m_ppTitle ){
        m_csTitle = *m_ppTitle;
    }
	CDialog::OnInitDialog();
	return TRUE;
}

void CPageTitleDlg::OnOK() 
{
	CDialog::OnOK();
    m_csTitle.TrimLeft();
    m_csTitle.TrimRight();

    // Return new string at location supplied
    if( m_ppTitle ){
        XP_FREEIF(*m_ppTitle);
        *m_ppTitle = XP_STRDUP(LPCSTR(m_csTitle));
    }
}

CPasteSpecialDlg::CPasteSpecialDlg(CWnd* pParent)
	: CDialog(CPasteSpecialDlg::IDD, pParent),
    m_iResult(0)
{
}

BEGIN_MESSAGE_MAP(CPasteSpecialDlg, CDialog)
	//{{AFX_MSG_MAP(CPasteSpecialDlg)
	ON_BN_CLICKED(IDC_PASTE_TEXT, OnPasteText)
	ON_BN_CLICKED(IDC_PASTE_IMAGE, OnPasteImage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CPasteSpecialDlg::OnInitDialog()
{
    m_ResourceSwitcher.Reset();
	CDialog::OnInitDialog();
	return TRUE;
}

void CPasteSpecialDlg::OnPasteImage()
{
    m_iResult = ED_PASTE_IMAGE;
	CDialog::OnOK();
}

void CPasteSpecialDlg::OnPasteText()
{
    m_iResult = ED_PASTE_TEXT;
	CDialog::OnOK();
}

/////////////////////////////////////////////////////////////////////////////
// CGetColumnsDlg dialog

CGetColumnsDlg::CGetColumnsDlg(CWnd* pParent)
	: CDialog(CGetColumnsDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEditHintDlg)
	m_iColumns = 1;
	//}}AFX_DATA_INIT
}

BEGIN_MESSAGE_MAP(CGetColumnsDlg, CDialog)
	//{{AFX_MSG_MAP(CGetColumnsDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CGetColumnsDlg::OnInitDialog() 
{
    m_ResourceSwitcher.Reset();
  	CDialog::OnInitDialog();
    return TRUE;
}

void CGetColumnsDlg::OnOK() 
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}

void CGetColumnsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGetColumnsDlg)
    DDX_Text(pDX, IDC_TABLE_COLUMNS, m_iColumns);
	DDV_MinMaxInt(pDX, m_iColumns, 1, 100 /*MAX_TABLE_COLUMNS*/);
	//}}AFX_DATA_MAP
}

