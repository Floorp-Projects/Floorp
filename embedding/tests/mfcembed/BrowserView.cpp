/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *   Rod Spears <rods@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// File Overview....
//
// When the CBrowserFrm creates this View:
//   - CreateBrowser() is called in OnCreate() to create the
//       mozilla embeddable browser
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
//      to AfxRegisterWndClass() inside of PreCreateWindow() below
//      If these flags are present then you'll see screen flicker when 
//      you resize the frame window
//
// Next suggested file to look at : BrowserImpl.cpp
//

#include "stdafx.h"
#include "MfcEmbed.h"
#include "BrowserView.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "Dialogs.h"
#include "CPageSetupPropSheet.h"

// Mozilla Includes
#include "nsIIOService.h"
#include "nsIWidget.h"
#include "nsIServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"
#include "nsXPCOM.h"

static nsresult
NewURI(nsIURI **result, const nsAString &spec)
{
  nsEmbedCString specUtf8;
  NS_UTF16ToCString(spec, NS_CSTRING_ENCODING_UTF8, specUtf8);

  nsCOMPtr<nsIIOService> ios = do_GetService("@mozilla.org/network/io-service;1");
  NS_ENSURE_TRUE(ios, NS_ERROR_UNEXPECTED);

  return ios->NewURI(specUtf8, nsnull, nsnull, result);
}

static void
ReplaceChar(nsAString &str, char oldChar, char newChar)
{
  // XXX this could be much more efficient

  PRUnichar *data = NS_StringCloneData(str);
  for (; *data; ++data)
  {
    if ((char ) *data == oldChar)
      *data = (PRUnichar) newChar;
  }
  NS_StringSetData(str, data);
  nsMemory::Free(data);
}

static void
StripChars(nsAString &str, const char *chars)
{
  // XXX this could be much more efficient

  PRUint32 len = str.Length();

  PRUnichar *data = NS_StringCloneData(str);
  PRUnichar *dataEnd = data + len;
  for (; *data; ++data)
  {
    if (strchr(chars, (char ) *data))
    {
      // include trailing null terminator in the memmove
      memmove(data, data + 1, (dataEnd - data) * sizeof(PRUnichar));
      --dataEnd;
    }
  }
  NS_StringSetData(str, data);
  nsMemory::Free(data);
}

static void
StripChars(nsACString &str, const char *chars)
{
  // XXX this could be much more efficient

  PRUint32 len = str.Length();

  char *data = NS_CStringCloneData(str);
  char *dataEnd = data + len;
  for (; *data; ++data)
  {
    if (strchr(chars, *data))
    {
      // include trailing null terminator in the memmove
      memmove(data, data + 1, dataEnd - data);
      --dataEnd;
    }
  }
  NS_CStringSetData(str, data);
  nsMemory::Free(data);
}

static const char* kWhitespace="\b\t\r\n ";

static void
CompressWhitespace(nsAString &str)
{
  const PRUnichar *p;
  PRInt32 i, len = (PRInt32) NS_StringGetData(str, &p);

  // trim leading whitespace

  for (i=0; i<len; ++i)
  {
    if (!strchr(kWhitespace, (char) p[i]))
      break;
  }

  if (i>0)
  {
    NS_StringCutData(str, 0, i);
    len = (PRInt32) NS_StringGetData(str, &p);
  }

  // trim trailing whitespace

  for (i=len-1; i>=0; --i)
  {
    if (!strchr(kWhitespace, (char) p[i]))
      break;
  }

  if (++i < len)
    NS_StringCutData(str, i, len - i);
}

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

    // UrlBar command handlers
    ON_COMMAND(IDOK, OnNewUrlEnteredInUrlBar)
    ON_CBN_SELENDOK(ID_URL_BAR, OnUrlSelectedInUrlBar)

    // Menu/Toolbar command handlers
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
    ON_COMMAND(ID_OPEN_LINK_IN_NEW_WINDOW, OnOpenLinkInNewWindow)
    ON_COMMAND(ID_VIEW_IMAGE, OnViewImageInNewWindow)
    ON_COMMAND(ID_COPY_LINK_LOCATION, OnCopyLinkLocation)
    ON_COMMAND(ID_SAVE_LINK_AS, OnSaveLinkAs)
    ON_COMMAND(ID_SAVE_IMAGE_AS, OnSaveImageAs)
    ON_COMMAND(ID_EDIT_FIND, OnShowFindDlg)
    ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
    ON_COMMAND(ID_FILE_PRINTPREVIEW, OnFilePrintPreview)
    ON_COMMAND(ID_FILE_PRINTSETUP, OnFilePrintSetup)
    ON_REGISTERED_MESSAGE(WM_FINDMSG, OnFindMsg)
    ON_COMMAND(ID_VIEW_FRAME_SOURCE, OnViewFrameSource)
    ON_COMMAND(ID_OPEN_FRAME_IN_NEW_WINDOW, OnOpenFrameInNewWindow)

    // Menu/Toolbar UI update handlers
    ON_UPDATE_COMMAND_UI(ID_NAV_BACK, OnUpdateNavBack)
    ON_UPDATE_COMMAND_UI(ID_NAV_FORWARD, OnUpdateNavForward)
    ON_UPDATE_COMMAND_UI(ID_NAV_STOP, OnUpdateNavStop)
    ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateCut)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateCopy)
    ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdatePaste)
    ON_UPDATE_COMMAND_UI(ID_FILE_PRINT, OnUpdateFilePrint)
    ON_UPDATE_COMMAND_UI(ID_FILE_PRINTSETUP, OnUpdatePrintSetup)
    ON_UPDATE_COMMAND_UI(ID_VIEW_IMAGE, OnUpdateViewImage)
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
    m_bUrlBarClipOp = FALSE;
    m_bCurrentlyPrinting = FALSE;

    m_SecurityState = SECURITY_STATE_INSECURE;

  m_InPrintPreview = FALSE;
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

    // If the browser window hosting chrome or content?
    dsti->SetItemType(((CMfcEmbedApp *)AfxGetApp())->m_bChrome ?
        nsIDocShellTreeItem::typeChromeWrapper :
        nsIDocShellTreeItem::typeContentWrapper);

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
        do_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, mpBrowserImpl)));
    (void)mWebBrowser->AddWebBrowserListener(weakling, 
                                NS_GET_IID(nsIWebProgressListener));

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

    return NS_OK;
}

BOOL CBrowserView::PreCreateWindow(CREATESTRUCT& cs) 
{
    if (!CWnd::PreCreateWindow(cs))
        return FALSE;

    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    cs.style &= ~WS_BORDER;
    cs.style |= WS_CLIPCHILDREN;
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
    return (strUrl.Find(_T("view-source:"), 0) != -1) ? TRUE : FALSE;
}

BOOL CBrowserView::OpenViewSourceWindow(const char* pUrl)
{
    nsEmbedString str;
    NS_CStringToUTF16(nsEmbedCString(pUrl), NS_CSTRING_ENCODING_ASCII, str);
    return OpenViewSourceWindow(str.get());
}

BOOL CBrowserView::OpenViewSourceWindow(const PRUnichar* pUrl)
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
    nsEmbedCString uriString;
    rv = currentURI->GetSpec(uriString);
    if(NS_FAILED(rv))
        return;

    // Build the view-source: url
    nsEmbedCString viewSrcUrl("view-source:");
    viewSrcUrl.Append(uriString);

    OpenViewSourceWindow(viewSrcUrl.get());
}

void CBrowserView::OnViewInfo() 
{
    AfxMessageBox(_T("To Be Done..."));
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
    PRUint32 loadFlags = nsIWebNavigation::LOAD_FLAGS_NONE;
    if (GetKeyState(VK_SHIFT))
        loadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
    if (mWebNav)
        mWebNav->Reload(loadFlags);
}

void CBrowserView::OnNavStop() 
{
    if(mWebNav)
        mWebNav->Stop(nsIWebNavigation::STOP_ALL);
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
    TCHAR *lpszFilter =
        _T("HTML Files Only (*.htm;*.html)|*.htm;*.html|")
        _T("All Files (*.*)|*.*||");

    CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    lpszFilter, this);
    if(cf.DoModal() == IDOK)
    {
        CString strFullPath = cf.GetPathName(); // Will be like: c:\tmp\junk.htm
        OpenURL(strFullPath);
    }
}

void CBrowserView::GetBrowserWindowTitle(nsAString& title)
{
    PRUnichar *idlStrTitle = nsnull;
    if(mBaseWindow)
        mBaseWindow->GetTitle(&idlStrTitle);

    title = idlStrTitle;
    nsMemory::Free(idlStrTitle);
}

void CBrowserView::OnFileSaveAs()
{
    nsEmbedString fileName;

    GetBrowserWindowTitle(fileName); // Suggest the window title as the filename

    // Sanitize the file name of all illegal characters

    USES_CONVERSION;
    CompressWhitespace(fileName);     // Remove whitespace from the ends
    StripChars(fileName, "\\*|:\"><?"); // Strip illegal characters
    ReplaceChar(fileName, '.', L'_');   // Dots become underscores
    ReplaceChar(fileName, '/', L'-');   // Forward slashes become hyphens

    TCHAR *lpszFilter =
        _T("Web Page, HTML Only (*.htm;*.html)|*.htm;*.html|")
        _T("Web Page, Complete (*.htm;*.html)|*.htm;*.html|")
        _T("Text File (*.txt)|*.txt||");

    CFileDialog cf(FALSE, _T("htm"), W2CT(fileName.get()), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    lpszFilter, this);

    if(cf.DoModal() == IDOK)
    {
        CString strFullPath = cf.GetPathName(); // Will be like: c:\tmp\junk.htm
        TCHAR *pStrFullPath = strFullPath.GetBuffer(0); // Get char * for later use
        
        BOOL bSaveAll = FALSE;        
        CString strDataPath; 
        TCHAR *pStrDataPath = NULL;
        if(cf.m_ofn.nFilterIndex == 2) 
        {
            // cf.m_ofn.nFilterIndex == 2 indicates
            // user want to save the complete document including
            // all frames, images, scripts, stylesheets etc.

            bSaveAll = TRUE;

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
        {
            nsEmbedCString fullPath(T2CA(pStrFullPath));
            nsCOMPtr<nsILocalFile> file;
            NS_NewNativeLocalFile(fullPath, TRUE, getter_AddRefs(file));

            nsCOMPtr<nsILocalFile> data;
            if (pStrDataPath)
            {
                nsEmbedCString dataPath(T2CA(pStrDataPath));
                NS_NewNativeLocalFile(dataPath, TRUE, getter_AddRefs(data));
            }

            if(bSaveAll)
                persist->SaveDocument(nsnull, file, data, nsnull, 0, 0);
            else
                persist->SaveURI(nsnull, nsnull, nsnull, nsnull, nsnull, file);
        }
    }
}

void CBrowserView::OpenURL(const char* pUrl)
{
    nsEmbedString str;
    NS_CStringToUTF16(nsEmbedCString(pUrl), NS_CSTRING_ENCODING_ASCII, str);
    OpenURL(str.get());
}

void CBrowserView::OpenURL(const PRUnichar* pUrl)
{
    if(mWebNav)
        mWebNav->LoadURI(pUrl,                              // URI string
                         nsIWebNavigation::LOAD_FLAGS_NONE, // Load flags
                         nsnull,                            // Referring URI
                         nsnull,                            // Post data
                         nsnull);                           // Extra headers
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

    PRUint32 size = mCtxMenuLinkUrl.Length() + 1;

    HGLOBAL hClipData = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);
    if(! hClipData)
        return;

    char *pszClipData = (char*)::GlobalLock(hClipData);
    if(!pszClipData)
        return;

    memcpy(pszClipData, mCtxMenuLinkUrl.get(), size);

    GlobalUnlock(hClipData);

    EmptyClipboard();
    SetClipboardData(CF_TEXT, hClipData);
    CloseClipboard();
}

void CBrowserView::OnOpenLinkInNewWindow()
{
    if(mCtxMenuLinkUrl.Length())
        OpenURLInNewWindow(mCtxMenuLinkUrl.get());
}

void CBrowserView::OnViewImageInNewWindow()
{
    if(mCtxMenuImgSrc.Length())
        OpenURLInNewWindow(mCtxMenuImgSrc.get());
}

void CBrowserView::OnUpdateViewImage(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(mCtxMenuImgSrc.Length() > 0);
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
    rv = NewURI(getter_AddRefs(linkURI), mCtxMenuLinkUrl);
    if (NS_FAILED(rv)) 
        return;

    // Get the "path" portion (see nsIURI.h for more info
    // on various parts of a URI)
    nsEmbedCString fileName;
    linkURI->GetPath(fileName);

    // The path may have the "/" char in it - strip those
    StripChars(fileName, "\\/");

    // Now, use this file name in a File Save As dlg...

    TCHAR *lpszFilter =
        _T("HTML Files (*.htm;*.html)|*.htm;*.html|")
        _T("Text Files (*.txt)|*.txt|")
        _T("All Files (*.*)|*.*||");

    CString strFilename = fileName.get();

    CFileDialog cf(FALSE, _T("htm"), strFilename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        lpszFilter, this);
    if(cf.DoModal() == IDOK)
    {
        USES_CONVERSION;
        nsEmbedCString fullPath; fullPath.Assign(T2CA(cf.GetPathName()));
        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(mWebBrowser));
        if(persist)
        {
            nsCOMPtr<nsILocalFile> file;
            NS_NewNativeLocalFile(fullPath, TRUE, getter_AddRefs(file));
            persist->SaveURI(linkURI, nsnull, nsnull, nsnull, nsnull, file);
        }
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
    rv = NewURI(getter_AddRefs(linkURI), mCtxMenuImgSrc);
    if (NS_FAILED(rv)) 
        return;

    // Get the "path" portion (see nsIURI.h for more info
    // on various parts of a URI)
    nsEmbedCString path;
    linkURI->GetPath(path);

    // The path may have the "/" char in it - strip those
    nsEmbedCString fileName(path);
    StripChars(fileName, "\\/");

    // Now, use this file name in a File Save As dlg...

    TCHAR *lpszFilter = _T("All Files (*.*)|*.*||");
    CString strFilename = fileName.get();

    CFileDialog cf(FALSE, NULL, strFilename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        lpszFilter, this);
    if(cf.DoModal() == IDOK)
    {
        USES_CONVERSION;
        nsEmbedCString fullPath; fullPath.Assign(T2CA(cf.GetPathName()));

        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(mWebBrowser));
        if(persist)
        {
            nsCOMPtr<nsILocalFile> file;
            NS_NewNativeLocalFile(fullPath, TRUE, getter_AddRefs(file));
            persist->SaveURI(linkURI, nsnull, nsnull, nsnull, nsnull, file);
        }
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
        PRUnichar *stringBuf = nsnull;
        finder->GetSearchString(&stringBuf);
        csSearchStr = stringBuf;
        nsMemory::Free(stringBuf);

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
//       ON_REGISTERED_MESSAGE(WM_FINDMSG, OnFindMsg) 
//
//    message map entry.
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
        USES_CONVERSION;
        finder->SetSearchString(T2W(dlg->GetFindString().GetBuffer(0)));
        finder->SetMatchCase(dlg->MatchCase() ? PR_TRUE : PR_FALSE);
        finder->SetEntireWord(dlg->MatchWholeWord() ? PR_TRUE : PR_FALSE);
        finder->SetWrapFind(dlg->WrapAround() ? PR_TRUE : PR_FALSE);
        finder->SetFindBackwards(dlg->SearchBackwards() ? PR_TRUE : PR_FALSE);

        PRBool didFind;
        nsresult rv = finder->FindNext(&didFind);
        
        if(!didFind)
        {
            AfxMessageBox(IDS_SRCH_STR_NOT_FOUND);
            dlg->SetFocus();
        }

        return (NS_SUCCEEDED(rv) && didFind);
    }

    return 0;
}

void CBrowserView::OnFilePrint()
{
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
    if(print)
    {
    if (!m_PrintSettings) 
    {
      if (NS_FAILED(print->GetGlobalPrintSettings(getter_AddRefs(m_PrintSettings)))) {
        return;
      }
    }
    nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
      if(print)
    {
      print->Print(m_PrintSettings, nsnull);
    }

  }
}

void CBrowserView::OnFilePrintPreview()
{
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
    if(print)
    {
    if (!m_PrintSettings) 
    {
      if (NS_FAILED(print->GetGlobalPrintSettings(getter_AddRefs(m_PrintSettings)))) {
        return;
      }
    }
    if (!m_InPrintPreview) 
    {
      if (NS_SUCCEEDED(print->PrintPreview(m_PrintSettings, nsnull, nsnull))) 
      {
        m_InPrintPreview = TRUE;

        CMenu* menu = mpBrowserFrame->GetMenu();
        if (menu) 
        {
          menu->CheckMenuItem( ID_FILE_PRINTPREVIEW, MF_CHECKED );
        }
      }
    } 
    else 
    {
      if (NS_SUCCEEDED(print->ExitPrintPreview())) 
      {
        m_InPrintPreview = FALSE;
        CMenu* menu = mpBrowserFrame->GetMenu();
        if (menu) 
        {
          menu->CheckMenuItem( ID_FILE_PRINTPREVIEW, MF_UNCHECKED );
        }
      }
    }
  }
}

void CBrowserView::OnFilePrintSetup()
{
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
    if(print)
  {
    if (!m_PrintSettings) 
    {
      if (NS_FAILED(print->GetGlobalPrintSettings(getter_AddRefs(m_PrintSettings)))) {
        return;
      }
    }
    CPageSetupPropSheet propSheet(_T("Page Setup"), this);

    propSheet.SetPrintSettingsValues(m_PrintSettings);

    int status = propSheet.DoModal();
    if (status == IDOK) 
    {
      propSheet.GetPrintSettingsValues(m_PrintSettings);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
void CBrowserView::OnUpdateFilePrint(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_bCurrentlyPrinting);
}

/////////////////////////////////////////////////////////////////////////////
void CBrowserView::OnUpdatePrintSetup(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_bCurrentlyPrinting && !m_InPrintPreview);
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
  if (mbDocumentLoading && !aBusy && m_InPrintPreview)
  {
      nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
        if(print)
        {
          PRBool isDoingPP;
          print->GetDoingPrintPreview(&isDoingPP);
          if (!isDoingPP) 
          {
              m_InPrintPreview = FALSE;
              CMenu* menu = mpBrowserFrame->GetMenu();
              if (menu) 
              {
                  menu->CheckMenuItem( ID_FILE_PRINTPREVIEW, MF_UNCHECKED );
              }
          }
      }
  }
    mbDocumentLoading = aBusy;
}

void CBrowserView::SetCtxMenuLinkUrl(nsEmbedString& strLinkUrl)
{
    mCtxMenuLinkUrl = strLinkUrl;
}

void CBrowserView::SetCtxMenuImageSrc(nsEmbedString& strImgSrc)
{
    mCtxMenuImgSrc = strImgSrc;
}

void CBrowserView::SetCurrentFrameURL(nsEmbedString& strCurrentFrameURL)
{
    mCtxMenuCurrentFrameURL = strCurrentFrameURL;
}

void CBrowserView::Activate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
    if(bMinimized)  // This isn't an activate event that Gecko cares about
        return;

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

void CBrowserView::ShowSecurityInfo()
{
    HWND hParent = mpBrowserFrame->m_hWnd;

    if(m_SecurityState == SECURITY_STATE_INSECURE) {
        CString csMsg;
        csMsg.LoadString(IDS_NOSECURITY_INFO);
        ::MessageBox(hParent, csMsg, _T("MfcEmbed"), MB_OK);
        return;
    }

    ::MessageBox(hParent, _T("To Be Done.........."), _T("MfcEmbed"), MB_OK);
}

// Determintes if the currently loaded document
// contains frames
//
BOOL CBrowserView::ViewContentContainsFrames()
{
    nsresult rv = NS_OK;

    // Get nsIDOMDocument from nsIWebNavigation
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = mWebNav->GetDocument(getter_AddRefs(domDoc));
    if(NS_FAILED(rv))
       return FALSE;

    // QI nsIDOMDocument for nsIDOMHTMLDocument
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
    if (!htmlDoc)
        return FALSE;
   
    // Get the <body> element of the doc
    nsCOMPtr<nsIDOMHTMLElement> body;
    rv = htmlDoc->GetBody(getter_AddRefs(body));
    if(NS_FAILED(rv))
       return FALSE;

    // Is it of type nsIDOMHTMLFrameSetElement?
    nsCOMPtr<nsIDOMHTMLFrameSetElement> frameset = do_QueryInterface(body);

    return (frameset != nsnull);
}

void CBrowserView::OnViewFrameSource()
{
    // Build the view-source: url
    //
    nsEmbedString viewSrcUrl;
    viewSrcUrl.Append(L"view-source:");
    viewSrcUrl.Append(mCtxMenuCurrentFrameURL);

    OpenViewSourceWindow(viewSrcUrl.get());
}

void CBrowserView::OnOpenFrameInNewWindow()
{
    if(mCtxMenuCurrentFrameURL.Length())
        OpenURLInNewWindow(mCtxMenuCurrentFrameURL.get());
}
