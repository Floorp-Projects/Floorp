/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

// File Overview....
//
// When the CBrowserFrm creates this View:
//   - CreateBrowser() is called in OnCreate() to create the
//	   mozilla embeddable browser
//
// OnSize() method handles the window resizes and calls the approriate
// interface method to resize the embedded browser properly
//
// Command handlers to handle browser navigation - OnNavBack(), 
// OnNavForward() etc
//
// DestroyBrowser() called for cleaning up during object destruction
//
// Some important coding notes....
//
// 1. Make sure we do not have the CS_HREDRAW|CS_VREDRAW in the call
//	  to AfxRegisterWndClass() inside of PreCreateWindow() below
//	  If these flags are present then you'll see screen flicker when 
//	  you resize the frame window
//
// Next suggested file to look at : BrowserImpl.cpp
//

#include "stdafx.h"
#include "MfcEmbed.h"
#include "BrowserView.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "Dialogs.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"

// Print Includes
#include "PrintProgressDialog.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Register message for FindDialog communication
static UINT WM_FINDMSG = ::RegisterWindowMessage(FINDMSGSTRING);

BEGIN_MESSAGE_MAP(CBrowserView, CWnd)
	//{{AFX_MSG_MAP(CBrowserView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_CBN_SELENDOK(ID_URL_BAR, OnUrlSelectedInUrlBar)
	ON_COMMAND(IDOK, OnNewUrlEnteredInUrlBar)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_COMMAND(ID_VIEW_SOURCE, OnViewSource)
	ON_COMMAND(ID_VIEW_INFO, OnViewInfo)
	ON_COMMAND(ID_NAV_BACK, OnNavBack)
	ON_COMMAND(ID_NAV_FORWARD, OnNavForward)
	ON_COMMAND(ID_NAV_HOME, OnNavHome)
	ON_COMMAND(ID_NAV_RELOAD, OnNavReload)
	ON_COMMAND(ID_NAV_STOP, OnNavStop)
	ON_COMMAND(ID_EDIT_CUT, OnCut)
	ON_COMMAND(ID_EDIT_COPY, OnCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnPaste)
    ON_COMMAND(ID_EDIT_UNDO, OnUndoUrlBarEditOp)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnSelectAll)
	ON_COMMAND(ID_EDIT_SELECT_NONE, OnSelectNone)
	ON_COMMAND(ID_COPY_LINK_LOCATION, OnCopyLinkLocation)
	ON_COMMAND(ID_OPEN_LINK_IN_NEW_WINDOW, OnOpenLinkInNewWindow)
	ON_COMMAND(ID_VIEW_IMAGE, OnViewImageInNewWindow)
	ON_COMMAND(ID_SAVE_LINK_AS, OnSaveLinkAs)
	ON_COMMAND(ID_SAVE_IMAGE_AS, OnSaveImageAs)
	ON_COMMAND(ID_EDIT_FIND, OnShowFindDlg)
	ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT, OnUpdateFilePrint)
	ON_REGISTERED_MESSAGE(WM_FINDMSG, OnFindMsg)
	ON_COMMAND(ID_TESTS_CHANGEURL, OnTestsChangeUrl)
	ON_COMMAND(ID_TESTS_GLOBALHISTORY, OnTestsGlobalHistory)
	ON_COMMAND(ID_TOOLS_REMOVEGHPAGE, OnToolsRemoveGHPage)
	ON_COMMAND(ID_TESTS_CREATEFILE, OnTestsCreateFile)
	ON_UPDATE_COMMAND_UI(ID_NAV_BACK, OnUpdateNavBack)
	ON_UPDATE_COMMAND_UI(ID_NAV_FORWARD, OnUpdateNavForward)
	ON_UPDATE_COMMAND_UI(ID_NAV_STOP, OnUpdateNavStop)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdatePaste)
	ON_COMMAND(ID_INTERFACES_NSIFILE, OnInterfacesNsifile)
	ON_COMMAND(ID_TESTS_CREATEPROFILE, OnTestsCreateprofile)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY, OnInterfacesNsishistory)
	ON_COMMAND(ID_VERIFYBUGS_70228, OnVerifybugs70228)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


CBrowserView::CBrowserView()
{
	mWebBrowser = nsnull;
	mBaseWindow = nsnull;
	mWebNav = nsnull;

	mpBrowserImpl = nsnull;
	mpBrowserFrame = nsnull;
	mpBrowserFrameGlue = nsnull;

	mbDocumentLoading = PR_FALSE;

	m_pFindDlg = NULL;
  m_pPrintProgressDlg = NULL;

  m_bUrlBarClipOp = FALSE;
  m_bCurrentlyPrinting = FALSE;

  char *theUrl = "http://www.aol.com/";
}

CBrowserView::~CBrowserView()
{
}

// This is a good place to create the embeddable browser
// instance
//
int CBrowserView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	CreateBrowser();

	return 0;
}

void CBrowserView::OnDestroy()
{
	DestroyBrowser();
}

// Create an instance of the Mozilla embeddable browser
//
HRESULT CBrowserView::CreateBrowser() 
{	   
	// Create web shell
	nsresult rv;
	mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
	if(NS_FAILED(rv))
		return rv;

	// Save off the nsIWebNavigation interface pointer 
	// in the mWebNav member variable which we'll use 
	// later for web page navigation
	//
	rv = NS_OK;
	mWebNav = do_QueryInterface(mWebBrowser, &rv);
	if(NS_FAILED(rv))
		return rv;

//	mSessionHistory = do_GetInterface(mWebBrowser, &rv); // de: added 5/11

	// Create the CBrowserImpl object - this is the object
	// which implements the interfaces which are required
	// by an app embedding mozilla i.e. these are the interfaces
	// thru' which the *embedded* browser communicates with the
	// *embedding* app
	//
	// The CBrowserImpl object will be passed in to the 
	// SetContainerWindow() call below
	//
	// Also note that we're passing the BrowserFrameGlue pointer 
	// and also the mWebBrowser interface pointer via CBrowserImpl::Init()
	// of CBrowserImpl object. 
	// These pointers will be saved by the CBrowserImpl object.
	// The CBrowserImpl object uses the BrowserFrameGlue pointer to 
	// call the methods on that interface to update the status/progress bars
	// etc.
	mpBrowserImpl = new CBrowserImpl();
	if(mpBrowserImpl == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	// Pass along the mpBrowserFrameGlue pointer to the BrowserImpl object
	// This is the interface thru' which the XP BrowserImpl code communicates
	// with the platform specific code to update status bars etc.
	mpBrowserImpl->Init(mpBrowserFrameGlue, mWebBrowser);
	mpBrowserImpl->AddRef();

    mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, mpBrowserImpl));

	rv = NS_OK;
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser, &rv);
	if(NS_FAILED(rv))
		return rv;
    dsti->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

    // Create the real webbrowser window
  
	// Note that we're passing the m_hWnd in the call below to InitWindow()
	// (CBrowserView inherits the m_hWnd from CWnd)
	// This m_hWnd will be used as the parent window by the embeddable browser
	//
	rv = NS_OK;
	mBaseWindow = do_QueryInterface(mWebBrowser, &rv);
	if(NS_FAILED(rv))
		return rv;

	// Get the view's ClientRect which to be passed in to the InitWindow()
	// call below
	RECT rcLocation;
	GetClientRect(&rcLocation);
	if(IsRectEmpty(&rcLocation))
	{
		rcLocation.bottom++;
		rcLocation.top++;
	}

	rv = mBaseWindow->InitWindow(nsNativeWidget(m_hWnd), nsnull,
		0, 0, rcLocation.right - rcLocation.left, rcLocation.bottom - rcLocation.top);
	rv = mBaseWindow->Create();

    // Register the BrowserImpl object to receive progress messages
	// These callbacks will be used to update the status/progress bars
    nsWeakPtr weakling(
        dont_AddRef(NS_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, mpBrowserImpl))));
    rv = mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
	
	if (NS_FAILED(rv))
	{
		AfxMessageBox("Web Progress Listener not added.");
		WriteToOutputFile("Web Progress Listener not added.\r\n");
	}
	else
	{
		AfxMessageBox("Web Progress Listener added.");
		WriteToOutputFile("Web Progress Listener added.\r\n");
	}

	// Finally, show the web browser window
	mBaseWindow->SetVisibility(PR_TRUE);

	return S_OK;
}

HRESULT CBrowserView::DestroyBrowser() 
{	   
	if(mBaseWindow)
	{
		mBaseWindow->Destroy();
        mBaseWindow = nsnull;
	}

	if(mpBrowserImpl)
	{
		mpBrowserImpl->Release();
		mpBrowserImpl = nsnull;
	}

	nsWeakPtr weakling(
    dont_AddRef(NS_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, mpBrowserImpl))));
	nsresult rv;
    rv = mWebBrowser->RemoveWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
	if (NS_FAILED(rv))
	{
		AfxMessageBox("Web Progress Listener not removed.");
		WriteToOutputFile("Web Progress Listener not removed.\r\n");
	}
	else
	{
		AfxMessageBox("Web Progress Listener removed.");
		WriteToOutputFile("Web Progress Listener removed.\r\n");
	}

	return NS_OK;
}

BOOL CBrowserView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);

	return TRUE;
}

// Adjust the size of the embedded browser
// in response to any container size changes
//
void CBrowserView::OnSize( UINT nType, int cx, int cy)
{
    mBaseWindow->SetPositionAndSize(0, 0, cx, cy, PR_TRUE);    
}

// Called by this object's creator i.e. the CBrowserFrame object
// to pass it's pointer to us
//
void CBrowserView::SetBrowserFrame(CBrowserFrame* pBrowserFrame)
{
	mpBrowserFrame = pBrowserFrame;
}

void CBrowserView::SetBrowserFrameGlue(PBROWSERFRAMEGLUE pBrowserFrameGlue)
{
	mpBrowserFrameGlue = pBrowserFrameGlue;
}

// A new URL was entered in the URL bar
// Get the URL's text from the Urlbar's (ComboBox's) EditControl 
// and navigate to that URL
//
void CBrowserView::OnNewUrlEnteredInUrlBar()
{
	// Get the currently entered URL
	CString strUrl;
	mpBrowserFrame->m_wndUrlBar.GetEnteredURL(strUrl);

    if(IsViewSourceUrl(strUrl))
        OpenViewSourceWindow(strUrl.GetBuffer(0));
    else
	    // Navigate to that URL
	    OpenURL(strUrl.GetBuffer(0));	

	// Add what was just entered into the UrlBar
	mpBrowserFrame->m_wndUrlBar.AddURLToList(strUrl);
}

// A URL has  been selected from the UrlBar's dropdown list
//
void CBrowserView::OnUrlSelectedInUrlBar()
{
	CString strUrl;
	
	mpBrowserFrame->m_wndUrlBar.GetSelectedURL(strUrl);

    if(IsViewSourceUrl(strUrl))
        OpenViewSourceWindow(strUrl.GetBuffer(0));
    else
    	OpenURL(strUrl.GetBuffer(0));
}

BOOL CBrowserView::IsViewSourceUrl(CString& strUrl)
{
    return (strUrl.Find("view-source:", 0) != -1) ? TRUE : FALSE;
}

BOOL CBrowserView::OpenViewSourceWindow(const char* pUrl)
{
	// Create a new browser frame in which we'll show the document source
	// Note that we're getting rid of the toolbars etc. by specifying
	// the appropriate chromeFlags
	PRUint32 chromeFlags =  nsIWebBrowserChrome::CHROME_WINDOW_BORDERS |
							nsIWebBrowserChrome::CHROME_TITLEBAR |
							nsIWebBrowserChrome::CHROME_WINDOW_RESIZE;
	CBrowserFrame* pFrm = CreateNewBrowserFrame(chromeFlags);
	if(!pFrm)
		return FALSE;

	// Finally, load this URI into the newly created frame
	pFrm->m_wndBrowserView.OpenURL(pUrl);

    pFrm->BringWindowToTop();

    return TRUE;
}

void CBrowserView::OnViewSource() 
{
	if(! mWebNav)
		return;

	// Get the URI object whose source we want to view.
    nsresult rv = NS_OK;
	nsCOMPtr<nsIURI> currentURI;
	rv = mWebNav->GetCurrentURI(getter_AddRefs(currentURI));
	if(NS_FAILED(rv) || !currentURI)
		return;

	// Get the uri string associated with the nsIURI object
	nsXPIDLCString uriString;
	rv = currentURI->GetSpec(getter_Copies(uriString));
	if(NS_FAILED(rv))
		return;

    // Build the view-source: url
    nsCAutoString viewSrcUrl;
    viewSrcUrl.Append("view-source:");
    viewSrcUrl.Append(uriString);

    OpenViewSourceWindow(viewSrcUrl.get());
}

void CBrowserView::OnViewInfo() 
{
	AfxMessageBox("To Be Done...");
}

void CBrowserView::OnNavBack() 
{
	if(mWebNav)
		mWebNav->GoBack();
}

void CBrowserView::OnUpdateNavBack(CCmdUI* pCmdUI)
{
	PRBool canGoBack = PR_FALSE;

    if (mWebNav)
        mWebNav->GetCanGoBack(&canGoBack);

	pCmdUI->Enable(canGoBack);
}

void CBrowserView::OnNavForward() 
{
	if(mWebNav)
		mWebNav->GoForward();
}

void CBrowserView::OnUpdateNavForward(CCmdUI* pCmdUI)
{
	PRBool canGoFwd = PR_FALSE;

    if (mWebNav)
        mWebNav->GetCanGoForward(&canGoFwd);

	pCmdUI->Enable(canGoFwd);
}

void CBrowserView::OnNavHome() 
{
    // Get the currently configured HomePage URL
    CString strHomeURL;
 	CMfcEmbedApp *pApp = (CMfcEmbedApp *)AfxGetApp();
	if(pApp)
      pApp->GetHomePage(strHomeURL);

    if(strHomeURL.GetLength() > 0)
        OpenURL(strHomeURL);	
}

void CBrowserView::OnNavReload() 
{
	if(mWebNav)
		mWebNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
}

void CBrowserView::OnNavStop() 
{	
	if(mWebNav)
		mWebNav->Stop();
}

void CBrowserView::OnUpdateNavStop(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(mbDocumentLoading);
}

void CBrowserView::OnCut()
{
    if(m_bUrlBarClipOp)
    {
        // We need to operate on the URLBar selection
        mpBrowserFrame->CutUrlBarSelToClipboard();
        m_bUrlBarClipOp = FALSE;
    }
    else
    {
	    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);

	    if(clipCmds)
		    clipCmds->CutSelection();
    }
}

void CBrowserView::OnUpdateCut(CCmdUI* pCmdUI)
{
	PRBool canCutSelection = PR_FALSE;

	nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
	if (clipCmds)
        clipCmds->CanCutSelection(&canCutSelection);

    if(!canCutSelection)
    {
        // Check to see if the Cut cmd is to cut the URL 
        // selection in the UrlBar
        if(mpBrowserFrame->CanCutUrlBarSelection())
        {
            canCutSelection = TRUE;
            m_bUrlBarClipOp = TRUE;
        }
    }

	pCmdUI->Enable(canCutSelection);
}

void CBrowserView::OnCopy()
{
    if(m_bUrlBarClipOp)
    {
        // We need to operate on the URLBar selection
        mpBrowserFrame->CopyUrlBarSelToClipboard();
        m_bUrlBarClipOp = FALSE;
    }
    else
    {
        // We need to operate on the web page content
	    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);

	    if(clipCmds)
		    clipCmds->CopySelection();
    }
}

void CBrowserView::OnUpdateCopy(CCmdUI* pCmdUI)
{
	PRBool canCopySelection = PR_FALSE;

	nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if (clipCmds)
        clipCmds->CanCopySelection(&canCopySelection);

    if(!canCopySelection)
    {
        // Check to see if the Copy cmd is to copy the URL 
        // selection in the UrlBar
        if(mpBrowserFrame->CanCopyUrlBarSelection())
        {
            canCopySelection = TRUE;
            m_bUrlBarClipOp = TRUE;
        }
    }

	pCmdUI->Enable(canCopySelection);
}

void CBrowserView::OnPaste()
{
    if(m_bUrlBarClipOp)
    {
        mpBrowserFrame->PasteFromClipboardToUrlBar();
        m_bUrlBarClipOp = FALSE;
    }
    else
    {
	    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);

	    if(clipCmds)
		    clipCmds->Paste();
    }    
}

void CBrowserView::OnUndoUrlBarEditOp()
{
    if(mpBrowserFrame->CanUndoUrlBarEditOp())
        mpBrowserFrame->UndoUrlBarEditOp();
}

void CBrowserView::OnUpdatePaste(CCmdUI* pCmdUI)
{
	PRBool canPaste = PR_FALSE;

	nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if (clipCmds)
        clipCmds->CanPaste(&canPaste);

    if(!canPaste)
    {
        if(mpBrowserFrame->CanPasteToUrlBar())
        {
            canPaste = TRUE;
            m_bUrlBarClipOp = TRUE;
        }
    }

	pCmdUI->Enable(canPaste);
}

void CBrowserView::OnSelectAll()
{
	nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);

	if(clipCmds)
		clipCmds->SelectAll();
}

void CBrowserView::OnSelectNone()
{
	nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);

	if(clipCmds)
		clipCmds->SelectNone();
}

void CBrowserView::OnFileOpen()
{
	char *lpszFilter =
        "HTML Files Only (*.htm;*.html)|*.htm;*.html|"
        "All Files (*.*)|*.*||";

	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
					lpszFilter, this);
	if(cf.DoModal() == IDOK)
	{
		CString strFullPath = cf.GetPathName(); // Will be like: c:\tmp\junk.htm
		OpenURL(strFullPath);
	}
}

void CBrowserView::GetBrowserWindowTitle(nsCString& title)
{
	nsXPIDLString idlStrTitle;
	if(mBaseWindow)
		mBaseWindow->GetTitle(getter_Copies(idlStrTitle));

	title.AssignWithConversion(idlStrTitle);

	// Sanitize the title of all illegal characters
    title.CompressWhitespace();     // Remove whitespace from the ends
    title.StripChars("\\*|:\"><?"); // Strip illegal characters
    title.ReplaceChar('.', L'_');   // Dots become underscores
    title.ReplaceChar('/', L'-');   // Forward slashes become hyphens
}

void CBrowserView::OnFileSaveAs()
{
	nsCString fileName;

	GetBrowserWindowTitle(fileName); // Suggest the window title as the filename

	char *lpszFilter =
        "Web Page, HTML Only (*.htm;*.html)|*.htm;*.html|"
        "Web Page, Complete (*.htm;*.html)|*.htm;*.html|" 
        "Text File (*.txt)|*.txt||";

	CFileDialog cf(FALSE, "htm", (const char *)fileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
					lpszFilter, this);

	if(cf.DoModal() == IDOK)
	{
		CString strFullPath = cf.GetPathName(); // Will be like: c:\tmp\junk.htm
		char *pStrFullPath = strFullPath.GetBuffer(0); // Get char * for later use
		
		CString strDataPath; 
		char *pStrDataPath = NULL;
		if(cf.m_ofn.nFilterIndex == 2) 
		{
			// cf.m_ofn.nFilterIndex == 2 indicates
			// user want to save the complete document including
			// all frames, images, scripts, stylesheets etc.

			int idxPeriod = strFullPath.ReverseFind('.');
			strDataPath = strFullPath.Mid(0, idxPeriod);
			strDataPath += "_files";

			// At this stage strDataPath will be of the form
			// c:\tmp\junk_files - assuming we're saving to a file
			// named junk.htm
			// Any images etc in the doc will be saved to a dir
			// with the name c:\tmp\junk_files

			pStrDataPath = strDataPath.GetBuffer(0); //Get char * for later use
		}

        // Save the file
        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(mWebBrowser));
		if(persist)
			persist->SaveDocument(nsnull, pStrFullPath, pStrDataPath);
	}
}

void CBrowserView::OpenURL(const char* pUrl)
{
    if(mWebNav)
        mWebNav->LoadURI(NS_ConvertASCIItoUCS2(pUrl).GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
}

void CBrowserView::OpenURL(const PRUnichar* pUrl)
{
    if(mWebNav)
        mWebNav->LoadURI(pUrl, nsIWebNavigation::LOAD_FLAGS_NONE);
}

CBrowserFrame* CBrowserView::CreateNewBrowserFrame(PRUint32 chromeMask, 
									PRInt32 x, PRInt32 y, 
									PRInt32 cx, PRInt32 cy,
									PRBool bShowWindow)
{
	CMfcEmbedApp *pApp = (CMfcEmbedApp *)AfxGetApp();
	if(!pApp)
		return NULL;

	return pApp->CreateNewBrowserFrame(chromeMask, x, y, cx, cy, bShowWindow);
}

void CBrowserView::OpenURLInNewWindow(const PRUnichar* pUrl)
{
	if(!pUrl)
		return;

	CBrowserFrame* pFrm = CreateNewBrowserFrame();
	if(!pFrm)
		return;

	// Load the URL into it...

	// Note that OpenURL() is overloaded - one takes a "char *"
	// and the other a "PRUniChar *". We're using the "PRUnichar *"
	// version here

	pFrm->m_wndBrowserView.OpenURL(pUrl);
}

void CBrowserView::LoadHomePage()
{
	OnNavHome();
}

void CBrowserView::OnCopyLinkLocation()
{
	if(! mCtxMenuLinkUrl.Length())
		return;

	if (! OpenClipboard())
		return;

	HGLOBAL hClipData = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, mCtxMenuLinkUrl.Length() + 1);
	if(! hClipData)
		return;

	char *pszClipData = (char*)::GlobalLock(hClipData);
	if(!pszClipData)
		return;

	mCtxMenuLinkUrl.ToCString(pszClipData, mCtxMenuLinkUrl.Length() + 1);

	GlobalUnlock(hClipData);

	EmptyClipboard();
	SetClipboardData(CF_TEXT, hClipData);
	CloseClipboard();
}

void CBrowserView::OnOpenLinkInNewWindow()
{
	if(mCtxMenuLinkUrl.Length())
		OpenURLInNewWindow(mCtxMenuLinkUrl.GetUnicode());
}

void CBrowserView::OnViewImageInNewWindow()
{
	if(mCtxMenuImgSrc.Length())
		OpenURLInNewWindow(mCtxMenuImgSrc.GetUnicode());
}

void CBrowserView::OnSaveLinkAs()
{
	if(! mCtxMenuLinkUrl.Length())
		return;

	// Try to get the file name part from the URL
	// To do that we first construct an obj which supports 
	// nsIRUI interface. Makes it easy to extract portions
	// of a URL like the filename, scheme etc. + We'll also
	// use it while saving this link to a file
	nsresult rv   = NS_OK;
	nsCOMPtr<nsIURI> linkURI;
	rv = NS_NewURI(getter_AddRefs(linkURI), mCtxMenuLinkUrl);
	if (NS_FAILED(rv)) 
		return;

	// Get the "path" portion (see nsIURI.h for more info
	// on various parts of a URI)
	nsXPIDLCString path;
	linkURI->GetPath(getter_Copies(path));

	// The path may have the "/" char in it - strip those
	nsCAutoString fileName(path);
	fileName.StripChars("\\/");

	// Now, use this file name in a File Save As dlg...

	char *lpszFilter =
        "HTML Files (*.htm;*.html)|*.htm;*.html|"
		"Text Files (*.txt)|*.txt|" 
	    "All Files (*.*)|*.*||";

	const char *pFileName = fileName.Length() ? fileName.get() : NULL;

	CFileDialog cf(FALSE, "htm", pFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		lpszFilter, this);
	if(cf.DoModal() == IDOK)
	{
		CString strFullPath = cf.GetPathName();

        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(mWebBrowser));
		if(persist)
			persist->SaveURI(linkURI, nsnull, strFullPath.GetBuffer(0));
	}
}

void CBrowserView::OnSaveImageAs()
{
	if(! mCtxMenuImgSrc.Length())
		return;

	// Try to get the file name part from the URL
	// To do that we first construct an obj which supports 
	// nsIRUI interface. Makes it easy to extract portions
	// of a URL like the filename, scheme etc. + We'll also
	// use it while saving this link to a file
	nsresult rv   = NS_OK;
	nsCOMPtr<nsIURI> linkURI;
	rv = NS_NewURI(getter_AddRefs(linkURI), mCtxMenuImgSrc);
	if (NS_FAILED(rv)) 
		return;

	// Get the "path" portion (see nsIURI.h for more info
	// on various parts of a URI)
	nsXPIDLCString path;
	linkURI->GetPath(getter_Copies(path));

	// The path may have the "/" char in it - strip those
	nsCAutoString fileName(path);
	fileName.StripChars("\\/");

	// Now, use this file name in a File Save As dlg...

	char *lpszFilter = "All Files (*.*)|*.*||";
	const char *pFileName = fileName.Length() ? fileName.get() : NULL;

	CFileDialog cf(FALSE, NULL, pFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		lpszFilter, this);
	if(cf.DoModal() == IDOK)
	{
		CString strFullPath = cf.GetPathName();

        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(mWebBrowser));
		if(persist)
			persist->SaveURI(linkURI, nsnull, strFullPath.GetBuffer(0));
	}
}

void CBrowserView::OnShowFindDlg() 
{
	// When the the user chooses the Find menu item
	// and if a Find dlg. is already being shown
	// just set focus to the existing dlg instead of
	// creating a new one
	if(m_pFindDlg)
	{
		m_pFindDlg->SetFocus();
		return;
	}

	CString csSearchStr;
	PRBool bMatchCase = PR_FALSE;
	PRBool bMatchWholeWord = PR_FALSE;
	PRBool bWrapAround = PR_FALSE;
	PRBool bSearchBackwards = PR_FALSE;

	// See if we can get and initialize the dlg box with
	// the values/settings the user specified in the previous search
	nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
	if(finder)
	{
		nsXPIDLString stringBuf;
		finder->GetSearchString(getter_Copies(stringBuf));
		csSearchStr = stringBuf.get();

		finder->GetMatchCase(&bMatchCase);
		finder->GetEntireWord(&bMatchWholeWord);
		finder->GetWrapFind(&bWrapAround);
		finder->GetFindBackwards(&bSearchBackwards);		
	}

	m_pFindDlg = new CFindDialog(csSearchStr, bMatchCase, bMatchWholeWord,
							bWrapAround, bSearchBackwards, this);
	m_pFindDlg->Create(TRUE, NULL, NULL, 0, this);
}

// This will be called whenever the user pushes the Find
// button in the Find dialog box
// This method gets bound to the WM_FINDMSG windows msg via the 
//
//	   ON_REGISTERED_MESSAGE(WM_FINDMSG, OnFindMsg) 
//
//	message map entry.
//
// WM_FINDMSG (which is registered towards the beginning of this file)
// is the message via which the FindDialog communicates with this view
//
LRESULT CBrowserView::OnFindMsg(WPARAM wParam, LPARAM lParam)
{
	nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
	if(!finder)
		return NULL;

	// Get the pointer to the current Find dialog box
	CFindDialog* dlg = (CFindDialog *) CFindReplaceDialog::GetNotifier(lParam);
	if(!dlg) 
		return NULL;

	// Has the user decided to terminate the dialog box?
	if(dlg->IsTerminating())
		return NULL;

	if(dlg->FindNext())
	{
		nsString searchString;
		searchString.AssignWithConversion(dlg->GetFindString().GetBuffer(0));
		finder->SetSearchString(searchString.GetUnicode());
	
		finder->SetMatchCase(dlg->MatchCase() ? PR_TRUE : PR_FALSE);
		finder->SetEntireWord(dlg->MatchWholeWord() ? PR_TRUE : PR_FALSE);
		finder->SetWrapFind(dlg->WrapAround() ? PR_TRUE : PR_FALSE);
		finder->SetFindBackwards(dlg->SearchBackwards() ? PR_TRUE : PR_FALSE);

		PRBool didFind;
		nsresult rv = finder->FindNext(&didFind);
		
		return (NS_SUCCEEDED(rv) && didFind);
	}

    return 0;
}

void CBrowserView::OnFilePrint()
{
  nsCOMPtr<nsIDOMWindow> domWindow;
	mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if(domWindow) {
	  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
	  if(print)
	  {
      CPrintProgressDialog  dlg(mWebBrowser, domWindow);

	    nsCOMPtr<nsIURI> currentURI;
	    nsresult rv = mWebNav->GetCurrentURI(getter_AddRefs(currentURI));
      if(NS_SUCCEEDED(rv) || currentURI) 
      {
	      nsXPIDLCString path;
	      currentURI->GetPath(getter_Copies(path));
        dlg.SetURI(path.get());
      }
      m_bCurrentlyPrinting = TRUE;
      dlg.DoModal();
      m_bCurrentlyPrinting = FALSE;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
void CBrowserView::OnUpdateFilePrint(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!m_bCurrentlyPrinting);
}



// Called from the busy state related methods in the 
// BrowserFrameGlue object
//
// When aBusy is TRUE it means that browser is busy loading a URL
// When aBusy is FALSE, it's done loading
// We use this to update our STOP tool bar button
//
// We basically note this state into a member variable
// The actual toolbar state will be updated in response to the
// ON_UPDATE_COMMAND_UI method - OnUpdateNavStop() being called
//
void CBrowserView::UpdateBusyState(PRBool aBusy)
{
	mbDocumentLoading = aBusy;
}

void CBrowserView::SetCtxMenuLinkUrl(nsAutoString& strLinkUrl)
{
	mCtxMenuLinkUrl = strLinkUrl;
}

void CBrowserView::SetCtxMenuImageSrc(nsAutoString& strImgSrc)
{
	mCtxMenuImgSrc = strImgSrc;
}

void CBrowserView::Activate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
	nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(mWebBrowser));
	if(!focus)
		return;
    
    switch(nState) {
        case WA_ACTIVE:
        case WA_CLICKACTIVE:
            focus->Activate();
            break;
        case WA_INACTIVE:
            focus->Deactivate();
            break;
        default:
            break;
    }
}

// de: Start QA test cases here
// *********************************************************
// *********************************************************

void CBrowserView::OnTestsChangeUrl() 
{
	char *theUrl = "http://www.aol.com/";
	CUrlDialog myDialog;
	nsresult rv;

//	nsCOMPtr<nsIURI> pURI;

	if (!mWebNav)
	{
		QAOutput("Web navigation object not found. Change URL test not performed.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		QAOutput("Begin Change URL test.", 1);
		strcpy(theUrl, myDialog.m_urlfield);
		mWebNav->LoadURI(NS_ConvertASCIItoUCS2(theUrl).GetUnicode(), 
						nsIWebNavigation::LOAD_FLAGS_NONE);
		WriteToOutputFile("\r\nLoadURI() method is called.");
		WriteToOutputFile("theUrl = ");
		WriteToOutputFile(theUrl);

/*
		char *uriSpec;
		// GetcurrentURI() declared in nsIP3PUI.idl
		// used with webNav obj in nsP3PObserverHTML.cpp, line 239
		// this will be used as an indep routine to verify the URI load
		rv = mWebNav->GetCurrentURI( getter_AddRefs( pURI ) );
		if(NS_FAILED(rv) || !pURI)
			AfxMessageBox("Bad result for GetCurrentURI().");

		rv = pURI->GetSpec(&uriSpec);;
		if (NS_FAILED(rv))
			AfxMessageBox("Bad result for GetSpec().");

		AfxMessageBox("Start URL validation test().");
		if (strcmp(uriSpec, theUrl) == 0)
		{
			WriteToOutputFile("Url loaded successfully. Test Passed.");	
		}
		else
		{
			WriteToOutputFile("Url didn't load successfully. Test Failed.");
		}
*/
		QAOutput("End Change URL test.", 1);
	}
	else
		QAOutput("Change URL test not executed.", 2);

}

// *********************************************************

void CBrowserView::OnTestsGlobalHistory() 
{
	// create instance of myHistory object. Call's XPCOM
	// service manager to pass the contract ID.

	char *theUrl = "http://www.bogussite.com/";
	CUrlDialog myDialog;

	PRBool theRetVal = PR_FALSE;
	nsresult rv;

	nsCOMPtr<nsIGlobalHistory> myHistory(do_GetService(NS_GLOBALHISTORY_CONTRACTID));

	if (!myHistory)
	{
		QAOutput("Couldn't find history object. No GH tests performed.", 2);
		return;
	}

	if (myDialog.DoModal() == IDOK)
	{
		QAOutput("Begin IsVisited() and AddPage() tests.", 2);

		strcpy(theUrl, myDialog.m_urlfield);

		WriteToOutputFile("theUrl = ");
		WriteToOutputFile(theUrl);

		// see if url is already in the GH file (pre-AddPage() test)
		myHistory->IsVisited(theUrl, &theRetVal);

		if (theRetVal)
			QAOutput("URL has been visited. Won't execute AddPage().", 2);
		else
		{
			QAOutput("URL hasn't been visited. Will execute AddPage().", 2);

			// adds a url to the global history file
			rv = myHistory->AddPage(theUrl);

			// prints addPage() results to output file
			if (NS_FAILED(rv))
			{
				QAOutput("Invalid results for AddPage(). Url not added. Test failed.", 1);
				return;
			}
			else
				QAOutput("Valid results for AddPage(). Url added. Test passed.", 1);

			// checks if url was visited (post-AddPage() test)
 			myHistory->IsVisited(theUrl, &theRetVal);

			if (theRetVal)
				QAOutput("URL is visited; post-AddPage() test. IsVisited() test passed.", 1);
			else
				QAOutput("URL isn't visited; post-AddPage() test. IsVisited() test failed.", 1);
		}
		QAOutput("End IsVisited() and AddPage() tests.", 2);
	}
	else
		QAOutput("IsVisited() and AddPage() tests not executed.", 2);
}



// *********************************************************

void CBrowserView::OnTestsCreateFile() 
{
   nsresult rv;
   PRBool exists;
   nsCOMPtr<nsILocalFile> theTestFile(do_GetService(NS_LOCAL_FILE_CONTRACTID));

    if (!theTestFile)
	{
		QAOutput("File object doesn't exist. No File tests performed.", 2);
		return;
	}


	QAOutput("Start Create File test.", 2);

	rv = theTestFile->InitWithPath("c:\\temp\\theFile.txt");
	rv = theTestFile->Exists(&exists);

	QAOutput("File doesn't exist. We'll create it.\r\n", 1);
	rv = theTestFile->Create(nsIFile::NORMAL_FILE_TYPE, 0777);
	RvTestResult(rv, "File Create() test", 2);
}

// *********************************************************

void CBrowserView::OnTestsCreateprofile() 
{
    CProfilesDlg    myDialog;
    nsresult        rv;

	if (myDialog.DoModal() == IDOK)
    {       
//      NS_WITH_SERVICE(nsIProfile, profileService, NS_PROFILE_CONTRACTID, &rv);
		nsCOMPtr<nsIProfile> theProfServ(do_GetService(NS_PROFILE_CONTRACTID));
		if (!theProfServ)
		{
		   QAOutput("Didn't get profile service. No profile tests performed.", 2);
		   return;
		}

	   QAOutput("Start Profile switch test.", 2);

	   QAOutput("Retrieved profile service.", 2);
       rv = theProfServ->SetCurrentProfile(myDialog.m_SelectedProfile.GetUnicode());
	   RvTestResult(rv, "SetCurrentProfile() (profile switching) test", 2);

	   QAOutput("End Profile switch test.", 2);
    }
	else
	   QAOutput("Profile switch test not executed.", 2);
	
}

// *********************************************************
// *********************************************************
//					TOOLS to help us


void CBrowserView::OnToolsRemoveGHPage() 
{
	char *theUrl = "http://www.bogussite.com/";
	CUrlDialog myDialog;
	PRBool theRetVal = PR_FALSE;
	nsresult rv;
	nsCOMPtr<nsIGlobalHistory> myGHistory(do_GetService(NS_GLOBALHISTORY_CONTRACTID));
	if (!myGHistory)
	{
		QAOutput("Could not get the global history object.", 2);
		return;
	}
	nsCOMPtr<nsIBrowserHistory> myHistory = do_QueryInterface(myGHistory, &rv);
	if(!NS_SUCCEEDED(rv)) {
		QAOutput("Could not get the history object.", 2);
		return;
	}
//	nsCOMPtr<nsIBrowserHistory> myHistory(do_GetInterface(myGHistory));


	if (myDialog.DoModal() == IDOK)
	{
		QAOutput("Begin URL removal from the GH file.", 2);
		strcpy(theUrl, myDialog.m_urlfield);

		myGHistory->IsVisited(theUrl, &theRetVal);
		if (theRetVal)
		{
			rv = myHistory->RemovePage(theUrl);
			RvTestResult(rv, "RemovePage() test (url removal from GH file)", 2);
		}
		else
		{
			QAOutput("The URL wasn't in the GH file.\r\n", 1);
		}
		QAOutput("End URL removal from the GH file.", 2);
	}
	else
		QAOutput("URL removal from the GH file not executed.", 2);
}

// ***********************************************************************
// ************************** Interface Tests ****************************
// ***********************************************************************

// nsIFile:

void CBrowserView::OnInterfacesNsifile() 
{
   nsCOMPtr<nsILocalFile> theTestFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
   nsCOMPtr<nsILocalFile> theFileOpDir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));

   nsresult rv;
   PRBool exists;

    if (!theTestFile)
 	{
		QAOutput("File object doesn't exist. No File tests performed.", 2);
		return;
	}
	if (!theFileOpDir)
 	{
		QAOutput("File object doesn't exist. No File tests performed.", 2);
		return;
	}

	QAOutput("Begin nsIFile tests.", 2);

	rv = theTestFile->InitWithPath("c:\\temp\\");
	RvTestResult(rv, "InitWithPath() test (initializing file path)", 2);

	rv = theTestFile->AppendRelativePath("myFile.txt");
	RvTestResult(rv, "AppendRelativePath() test (append file to the path)", 2);

	rv = theTestFile->Exists(&exists);
	if (!exists)
	{
		QAOutput("File doesn't exist. We'll try creating it.", 2);
		rv = theTestFile->Create(nsIFile::NORMAL_FILE_TYPE, 0777);
		RvTestResult(rv, " File Create() test ('myFile.txt')", 2);
	}
	else
		QAOutput("File already exists (myFile.txt). We won't create it.", 2);


	rv = theTestFile->Exists(&exists);
	if (!exists)
		QAOutput("Exists() test Failed. File (myFile.txt) doesn't exist.", 2);
	else
		QAOutput("Exists() test Passed. File (myFile.txt) exists.", 2);

	// FILE COPY test

	QAOutput("Start File Copy test.", 2);

	rv = theFileOpDir->InitWithPath("c:\\temp\\");
	if (NS_FAILED(rv))
		QAOutput("The target dir wasn't found.", 2);
	else
		QAOutput("The target dir was found.", 2);

	rv = theTestFile->InitWithPath("c:\\temp\\myFile.txt");
	if (NS_FAILED(rv))
		QAOutput("The path wasn't found.", 2);
	else
		QAOutput("The path was found.", 2);

	rv = theTestFile->CopyTo(theFileOpDir, "myFile2.txt");
	RvTestResult(rv, "rv CopyTo() test", 2);

	rv = theTestFile->InitWithPath("c:\\temp\\myFile2.txt");
	rv = theTestFile->Exists(&exists);
	if (!exists)
		QAOutput("File didn't copy. CopyTo() test Failed.", 2);
	else
		QAOutput("File copied. CopyTo() test Passed.", 2);	

		// FILE MOVE test

	QAOutput("Start File Move test.", 2);

	rv = theFileOpDir->InitWithPath("c:\\Program Files\\");
	if (NS_FAILED(rv))
		QAOutput("The target dir wasn't found.", 2);

	rv = theTestFile->InitWithPath("c:\\temp\\myFile2.txt");
	if (NS_FAILED(rv))
		QAOutput("The path wasn't found.", 2);

	rv = theTestFile->MoveTo(theFileOpDir, "myFile2.txt");
	RvTestResult(rv, "MoveTo() test", 2);

	rv = theTestFile->InitWithPath("c:\\Program Files\\myFile2.txt");
	rv = theTestFile->Exists(&exists);
	if (!exists)
		QAOutput("File wasn't moved. MoveTo() test Failed.", 2);
	else
		QAOutput("File was moved. MoveTo() test Passed.", 2);	

	QAOutput("End nsIFile tests.", 2);
	
}

// ***********************************************************************
// nsISHistory & nsIHistoryEntry ifaces:

void CBrowserView::OnInterfacesNsishistory() 
{
   nsresult rv;
   PRInt32 numEntries = 5;
   PRInt32 theIndex;
   PRInt32 theMaxLength = 100;
   nsXPIDLString theTitle;
   PRBool isSubFrame;
   char * uriSpec;
   const char *  titleCString;
   CString strMsg;
   CString shString;

   nsCOMPtr<nsISHistory> theSessionHistory;
   nsCOMPtr<nsIHistoryEntry> theHistoryEntry;
   nsCOMPtr<nsIURI> theUri;
   // do_QueryInterface
   // NS_HISTORYENTRY_CONTRACTID
   // NS_SHISTORYLISTENER_CONTRACTID


   // addSHistoryListener test
	nsWeakPtr weakling(
        dont_AddRef(NS_GetWeakReference(NS_STATIC_CAST(nsISHistoryListener*, mpBrowserImpl))));
	rv = mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsISHistoryListener));

	if (NS_FAILED(rv))
		QAOutput("NSISHistory Listener not added.", 2);
	else
		QAOutput("NSISHistory Listener added.", 2);

	// get Session History through web nav iface
   if (mWebNav)
   {
		mWebNav->GetSessionHistory( getter_AddRefs(theSessionHistory));
   }

   if (!theSessionHistory)
   {
	   QAOutput("theSessionHistory object wasn't created. No session history tests performed.", 2);
	   return;
   }
   else
	   QAOutput("theSessionHistory object was created.", 2);


		// test count attribute in nsISHistory.idl
    rv = theSessionHistory->GetCount(&numEntries);
	RvTestResult(rv, "GetCount() (count attribute) test", 2);

		// test index attribute in nsISHistory.idl
	rv = theSessionHistory->GetIndex(&theIndex);
	RvTestResult(rv, "GetIndex() (index attribute) test", 2);

		// test maxLength attribute in nsISHistory.idl
	rv = theSessionHistory->SetMaxLength(theMaxLength);
	RvTestResult(rv, "SetMaxLength() (MaxLength attribute) test", 2);
	rv = theSessionHistory->GetMaxLength(&theMaxLength);
	RvTestResult(rv, "GetMaxLength() (MaxLength attribute) test", 2);

	    // getEntryAtIndex() tests

	QAOutput("Start nsiHistoryEntry tests.", 2);
	for (theIndex = 0; theIndex < numEntries; theIndex++)
	{
		strMsg.Format("the index = %d", theIndex); 
		AfxMessageBox(strMsg); 
		theSessionHistory->GetEntryAtIndex(theIndex, PR_FALSE, getter_AddRefs(theHistoryEntry));

			// nsiHistoryEntry.idl tests
		if (theHistoryEntry)
		{
			QAOutput("We have the History Entry object.", 2);	

			// test URI attribute in nsIHistoryEntry.idl
			rv = theHistoryEntry->GetURI(getter_AddRefs(theUri));
			RvTestResult(rv, "GetURI() (URI attribute) test", 2);

			// test title attribute in nsIHistoryEntry.idl
			rv = theHistoryEntry->GetTitle(getter_Copies(theTitle));
			RvTestResult(rv, "GetTitle() (title attribute) test", 2);
			titleCString = NS_ConvertUCS2toUTF8(theTitle).get();
			strMsg.Format("The title = %s", titleCString); 
			AfxMessageBox(strMsg);
			//Recycle(titleCString);

			// test isSubFrame attribute in nsIHistoryEntry.idl
			rv = theHistoryEntry->GetIsSubFrame(&isSubFrame);
			RvTestResult(rv, "GetIsSubFrame() (isSubFrame attribute) test", 2);
			strMsg.Format("The subFrame value = %d", isSubFrame); 
			AfxMessageBox(strMsg);

			if (theUri) 
			{
				QAOutput("We got the uri.", 2);
				rv = theUri->GetSpec(&uriSpec);
				if (NS_FAILED(rv))
					QAOutput("We didn't get the uriSpec.", 2);
				else
				{
					shString = "The SH URL = "; 
					shString += uriSpec; 
					AfxMessageBox(shString);
					CBrowserView::WriteToOutputFile("The SH URL = ");
					CBrowserView::WriteToOutputFile(uriSpec);
				}
				// Get the spec which is a char * 
				// Print out the url and title
			}
			else
				QAOutput("We didn't get the uri.", 2);
		}
		else
			QAOutput("We didn't get the History Entry.", 2);
	}	// end for loop


	   // test SHistoryEnumerator attribute in nsISHistory.idl
      nsCOMPtr<nsISimpleEnumerator> theSimpleEnum;

	  rv = theSessionHistory->GetSHistoryEnumerator(getter_AddRefs(theSimpleEnum));
	  RvTestResult(rv, "GetSHistoryEnumerator() (SHistoryEnumerator attribute) test", 2);

	  PRBool bMore = PR_FALSE;
	  nsCOMPtr<nsISupports> nextObj;
      nsCOMPtr<nsIHistoryEntry> nextHistoryEntry;
//	  do

	  while (NS_SUCCEEDED(theSimpleEnum->HasMoreElements(&bMore)) && bMore)
	  {
	     theSimpleEnum->GetNext(getter_AddRefs(nextObj));
		 if (!nextObj)
			continue;
		 nextHistoryEntry = do_QueryInterface(nextObj);
		 if (!nextHistoryEntry)
			continue;
		 rv = nextHistoryEntry->GetURI(getter_AddRefs(theUri));
		 rv = theUri->GetSpec(&uriSpec);
	   	 CString shString = "The SimpleEnum URL = "; 
		 shString += uriSpec; 
		 AfxMessageBox(shString);
//		 rv = theSimpleEnum->HasMoreElements(&bMore);		 
	  } 
//	  while(bMore);

	   // PurgeHistory() test
   if (numEntries > 0)
   {
	   strMsg.Format("numEntries = %d", numEntries); 
	   AfxMessageBox(strMsg); 

	   rv = theSessionHistory->PurgeHistory(numEntries);
	   if (NS_FAILED(rv))
		   QAOutput("Purge History test failed.", 2);
	   else
		   QAOutput("Purge History test succeeded.", 2);
   }
   else
		 QAOutput("numEntries not valid.", 2);

      // RemoveSHistoryListener test
/*
       nsWeakPtr weakling(
       dont_AddRef(NS_GetWeakReference(NS_STATIC_CAST(nsISHistoryListener*, mpBrowserImpl))));
	   rv = theSessionHistory->RemoveSHistoryListener(weakling);
*/
}

// ***********************************************************************
// ***************** Bug Verifications ******************
// ***********************************************************************

void CBrowserView::OnVerifybugs70228() 
{
	QAOutput("Not implemented yet.", 0);
/*
	nsCOMPtr<nsIHelperAppLauncherDialog> 
			myHALD(do_GetService(NS_IHELPERAPPLAUNCHERDLG_CONTRACTID));
	if (!myHALD)
		QAOutput("Object not created. It's NOT a service!", 2);
	else
		QAOutput("Object is created. But should it?! It's NOT a service!", 2);

	nsCOMPtr<nsIHelperAppLauncherDialog> 
			myHALD(do_CreateInstance(NS_IHELPERAPPLAUNCHERDLG_CONTRACTID));
	if (!myHALD)
		QAOutput("Object not created. It should be. It's a component!", 2);
	else
		QAOutput("Object is created. It's a component!", 2);	
*/
/*
nsCOMPtr<nsIHelperAppLauncher> 
			myHAL(do_CreateInstance(NS_IHELPERAPPLAUNCHERDLG_CONTRACTID));

	rv = myHALD->show(myHal, mySupp);
*/	
}

// ***********************************************************************
// ***************** Local QA Methods ******************
// ***********************************************************************

void CBrowserView::WriteToOutputFile(char *pLine) 
{ 
    CStdioFile myFile; 
    CFileException e; 
    CString strFileName = "c:\\temp\\MFCoutfile"; 

    if(! myFile.Open( strFileName, CStdioFile::modeCreate | CStdioFile::modeWrite
								| CStdioFile::modeNoTruncate, &e ) ) 
    { 
        CString failCause = "Unable to open file. Reason : "; 
        failCause += e.m_cause; 
        AfxMessageBox(failCause); 
    } 
    else 
    { 
		myFile.SeekToEnd();
        CString strLine = pLine; 
        strLine += "\r\n"; 

        myFile.WriteString(strLine); 

        myFile.Close(); 
    } 
} 

void CBrowserView::RvTestResult(nsresult rv, char *pLine, int displayMethod)
{
	// note: default displayMethod = 1 in .h file

   CString strLine = pLine;
   char theOutputLine[100];

   if (NS_FAILED(rv))
	   strLine += " failed.";
   else
	   strLine += " passed.";

   strcpy(theOutputLine, strLine);
   QAOutput(theOutputLine, displayMethod);
}

void CBrowserView::QAOutput(char *pLine, int displayMethod)
{
	// note: default displayMethod = 1 in .h file
//#if 0
   CString strLine = pLine;

   if (displayMethod == 0)
	   AfxMessageBox(strLine);
   else if (displayMethod == 1)
	   WriteToOutputFile(pLine);
   else 
   {
       WriteToOutputFile(pLine);
	   AfxMessageBox(strLine);
   }
//#endif
}

