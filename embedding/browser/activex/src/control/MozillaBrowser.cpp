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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "stdafx.h"
#include <string.h>
#include <string>
#include <objidl.h>
//#include <comdef.h>
#include <shlobj.h>

#include "MozillaControl.h"
#include "MozillaBrowser.h"
#include "IEHtmlDocument.h"
#include "PropertyDlg.h"
#include "PromptService.h"

#include "nsCWebBrowser.h"
#include "nsILocalFile.h"
#include "nsISelectionController.h"
#include "nsIWebBrowserPersist.h"
#include "nsIClipboardCommands.h"
#include "nsIProfile.h"
#include "nsIPrintListener.h"
#include "nsIPrintOptions.h"
#include "nsIWebBrowserPrint.h"
#include "nsIWidget.h"

#include "nsIDOMWindowInternal.h"
#include "nsIDOMHTMLAnchorElement.h"

#include "nsEmbedAPI.h"

#define HACK_NON_REENTRANCY
#ifdef HACK_NON_REENTRANCY
static HANDLE s_hHackedNonReentrancy = NULL;
#endif

#define NS_PROMPTSERVICE_CID \
  {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}

static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

// Macros to return errors from bad calls to the automation
// interfaces and sets a descriptive string on IErrorInfo so VB programmers
// have a clue why the call failed.

static const TCHAR *c_szInvalidArg = _T("Invalid parameter");
static const TCHAR *c_szUninitialized = _T("Method called while control is uninitialized");

#define RETURN_ERROR(message, hr) \
    SetErrorInfo(message, hr); \
    return hr;

#define RETURN_E_INVALIDARG() \
    RETURN_ERROR(c_szInvalidArg, E_INVALIDARG);

#define RETURN_E_UNEXPECTED() \
    RETURN_ERROR(c_szUninitialized, E_UNEXPECTED);

class PrintListener : public nsIPrintListener
{
    PRBool mComplete;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINTLISTENER

    PrintListener();
    virtual ~PrintListener();

    void WaitForComplete();
};


// Prefs

static const char *c_szPrefsHomePage = "browser.startup.homepage";
static const char *c_szDefaultPage   = "resource:/res/MozillaControl.html";

// Registry keys and values

static const TCHAR *c_szIEHelperObjectKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects");

// Some recent SDKs define these IOleCommandTarget groups, so they're
// postfixed with _Moz to prevent linker errors.

GUID CGID_IWebBrowser_Moz =
    { 0xED016940L, 0xBD5B, 0x11cf, {0xBA, 0x4E, 0x00, 0xC0, 0x4F, 0xD7, 0x08, 0x16} };

GUID CGID_MSHTML_Moz =
    { 0xED016940L, 0xBD5B, 0x11cf, {0xBA, 0x4E, 0x00, 0xC0, 0x4F, 0xD7, 0x08, 0x16} };

/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser


nsVoidArray CMozillaBrowser::sBrowserList;

//
// Constructor
//
CMozillaBrowser::CMozillaBrowser()
{
    NG_TRACE_METHOD(CMozillaBrowser::CMozillaBrowser);

    // ATL flags ensures the control opens with a window
    m_bWindowOnly = TRUE;
    m_bWndLess = FALSE;

    // Initialize layout interfaces
    mWebBrowserAsWin = nsnull;
    mPrefs = nsnull;
    mValidBrowserFlag = FALSE;

    // Ready state of control
    mBrowserReadyState = READYSTATE_UNINITIALIZED;
    
    // Create the container that handles some things for us
    mWebBrowserContainer = NULL;
    mEditor = NULL;

    // Controls starts off unbusy
    mBusyFlag = FALSE;

    // Control starts off in non-edit mode
    mEditModeFlag = FALSE;

    // Control starts off without being a drop target
    mHaveDropTargetFlag = FALSE;

     // the IHTMLDocument, lazy allocation.
     mIERootDocument = NULL;

    // Browser helpers
    mBrowserHelperList = NULL;
    mBrowserHelperListCount = 0;

    // Initialise the web shell
    Initialize();
}


//
// Destructor
//
CMozillaBrowser::~CMozillaBrowser()
{
    NG_TRACE_METHOD(CMozillaBrowser::~CMozillaBrowser);
    
    // Close the web shell
    Terminate();
}


STDMETHODIMP CMozillaBrowser::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_IWebBrowser,
        &IID_IWebBrowser2,
        &IID_IWebBrowserApp
    };
    for (int i=0;i<(sizeof(arr)/sizeof(arr[0]));i++)
    {
        if (::ATL::InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

//
// ShowContextMenu
//
void CMozillaBrowser::ShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    POINT pt;
    GetCursorPos(&pt);

    // Give the client application the chance to show its own menu
    // in place of the one the control is about to show.

    CIPtr(IDocHostUIHandler) spIDocHostUIHandler = m_spClientSite;
    if (spIDocHostUIHandler)
    {
        enum IE4MenuContexts
        {
            ctxMenuDefault = 0,
            ctxMenuImage,
            ctxMenuControl,
            ctxMenuTable,
            ctxMenuDebug,
            ctxMenu1DSelect,
            ctxMenuAnchor,
            ctxMenuImgDynSrc
        };

        DWORD dwID = ctxMenuDefault;
        if (aContextFlags & nsIContextMenuListener::CONTEXT_DOCUMENT)
        {
            dwID = ctxMenuDefault;
        }
        else if (aContextFlags & nsIContextMenuListener::CONTEXT_LINK)
        {
            dwID = ctxMenuAnchor;
        }
        else if (aContextFlags & nsIContextMenuListener::CONTEXT_IMAGE)
        {
            dwID = ctxMenuImage;
        }
        else if (aContextFlags & nsIContextMenuListener::CONTEXT_TEXT)
        {
            dwID = ctxMenu1DSelect;
        }
        else
        {
            dwID = ctxMenuDefault;
        }

        HRESULT hr = spIDocHostUIHandler->ShowContextMenu(dwID, &pt, NULL, NULL);
        if (hr == S_OK)
        {
            // Client handled menu
            return;
        }
    }

    LPTSTR pszMenuResource = NULL;
    if (aContextFlags & nsIContextMenuListener::CONTEXT_DOCUMENT)
    {
        pszMenuResource = MAKEINTRESOURCE(IDR_POPUP_DOCUMENT);
    }
    else if (aContextFlags & nsIContextMenuListener::CONTEXT_LINK)
    {
        pszMenuResource = MAKEINTRESOURCE(IDR_POPUP_LINK);
    }
    else if (aContextFlags & nsIContextMenuListener::CONTEXT_IMAGE)
    {
        pszMenuResource = MAKEINTRESOURCE(IDR_POPUP_IMAGE);
    }
    else if (aContextFlags & nsIContextMenuListener::CONTEXT_TEXT)
    {
        pszMenuResource = MAKEINTRESOURCE(IDR_POPUP_TEXT);
    }
    else
    {
        pszMenuResource = MAKEINTRESOURCE(IDR_POPUP_DOCUMENT);
    }

    if (pszMenuResource)
    {
        HMENU hMenu = LoadMenu(_Module.m_hInstResource, pszMenuResource);
        HMENU hPopupMenu = GetSubMenu(hMenu, 0);
        mContextNode = do_QueryInterface(aNode);
        UINT nCmd = TrackPopupMenu(hPopupMenu, TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, m_hWnd, NULL);
        DestroyMenu(hMenu);
        if (nCmd != 0)
        {
            SendMessage(WM_COMMAND, nCmd);
        }
        mContextNode = nsnull;
    }
}


//
// ShowURIPropertyDlg
//
void CMozillaBrowser::ShowURIPropertyDlg(const nsString &aURI)
{
    CPropertyDlg dlg;
    CPPageDlg linkDlg;
    dlg.AddPage(&linkDlg);

    if (aURI.Length() > 0)
    {
        USES_CONVERSION;
        linkDlg.mProtocol = "HyperText Transfer Protocol"; // TODO
        linkDlg.mType = "HTML Document"; // TODO
        linkDlg.mURL.AssignWithConversion(aURI);
    }

    dlg.DoModal();
}


//
// Displays a message box to the user. If the container provides
// a IDocHostShowUI interface we use that to display messages, otherwise
// a simple message box is shown.
//
int CMozillaBrowser::MessageBox(LPCTSTR lpszText, LPCTSTR lpszCaption, UINT nType)
{
    // Let the doc host display it's own message box if it can
    CIPtr(IDocHostShowUI) spIDocHostShowUI = m_spClientSite;
    if (spIDocHostShowUI)
    {
        USES_CONVERSION;
        LRESULT lResult = 0;
        HRESULT hr = spIDocHostShowUI->ShowMessage(m_hWnd,
                        T2OLE(lpszText), T2OLE(lpszCaption), nType, NULL, 0, &lResult);
        if (hr == S_OK)
        {
            return lResult;
        }
    }

    // Do the default message box
    return CWindow::MessageBox(lpszText, lpszCaption, nType);
}


//
// Sets the startup error message from a resource string
//
HRESULT CMozillaBrowser::SetStartupErrorMessage(UINT nStringID)
{
    TCHAR szMsg[1024];
    ::LoadString(_Module.m_hInstResource, nStringID, szMsg, sizeof(szMsg) / sizeof(szMsg[0]));
    mStartupErrorMessage = szMsg;
    return S_OK;
}


//
// Sets error information for VB programmers who want to know why
// something failed.
//
HRESULT CMozillaBrowser::SetErrorInfo(LPCTSTR lpszDesc, HRESULT hr)
{
    USES_CONVERSION;
    return AtlSetErrorInfo(GetObjectCLSID(), T2OLE(lpszDesc), 0, NULL, GUID_NULL, hr, NULL);
}


///////////////////////////////////////////////////////////////////////////////
// Message handlers


// Handle WM_CREATE windows message
LRESULT CMozillaBrowser::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnCreate);

    // Clip the child windows out of paint operations
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_CLIPCHILDREN);

    // Turn on the 3d border
//    SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) | WS_EX_CLIENTEDGE);

    // Create the NGLayout WebShell
    CreateBrowser();

    // TODO create and register a drop target

    // Control is ready
    mBrowserReadyState = READYSTATE_COMPLETE;
    FireOnChanged(DISPID_READYSTATE);

    // Load browser helpers
    LoadBrowserHelpers();

    // Browse to a default page - if in design mode
    BOOL bUserMode = FALSE;
    if (SUCCEEDED(GetAmbientUserMode(bUserMode)))
    {
        if (!bUserMode)
        {
            USES_CONVERSION;
            Navigate(A2OLE(c_szDefaultPage), NULL, NULL, NULL, NULL);
        }
    }

    return 0;
}


// Handle WM_DESTROY window message
LRESULT CMozillaBrowser::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnDestroy);

    // Unload browser helpers
    UnloadBrowserHelpers();

    // Clean up the browser
    DestroyBrowser();

    return 0;
}


// Handle WM_SIZE windows message
LRESULT CMozillaBrowser::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnSize);

    RECT rc;
    rc.top = 0;
    rc.left = 0;
    rc.right = LOWORD(lParam);
    rc.bottom = HIWORD(lParam);
    
    AdjustWindowRectEx(&rc, GetWindowLong(GWL_STYLE), FALSE, GetWindowLong(GWL_EXSTYLE));

    rc.right -= rc.left;
    rc.bottom -= rc.top;
    rc.left = 0;
    rc.top = 0;

    // Pass resize information down to the WebShell...
    if (mWebBrowserAsWin)
    {
        mWebBrowserAsWin->SetPosition(rc.left, rc.top);
        mWebBrowserAsWin->SetSize(rc.right - rc.left, rc.bottom - rc.top, PR_TRUE);
    }
    return 0;
}


// Handle WM_PAINT windows message (and IViewObject::Draw) 
HRESULT CMozillaBrowser::OnDraw(ATL_DRAWINFO& di)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnDraw);

    if (!IsValid())
    {
        RECT& rc = *(RECT*)di.prcBounds;
        DrawText(di.hdcDraw, mStartupErrorMessage.c_str(), -1, &rc, DT_TOP | DT_LEFT | DT_WORDBREAK);
    }

    return S_OK;
}

// Handle ID_PAGESETUP command
LRESULT CMozillaBrowser::OnPageSetup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnPageSetup);
    MessageBox(_T("No page setup yet!"), _T("Control Message"), MB_OK);

    // TODO show the page setup dialog

    return 0;
}

// Handle ID_PRINT command
LRESULT CMozillaBrowser::OnPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnPrint);
    if (!IsValid())
    {
        return 0;
    }
    PrintDocument(TRUE);
    return 0;
}

LRESULT CMozillaBrowser::OnSaveAs(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnSaveAs);

    OPENFILENAME SaveFileName;

    char szFile[_MAX_PATH];
    char szFileTitle[256];
    BSTR pageName = NULL;

    //TODO:    The IE control allows you to also save as "Web Page, complete"
    //      where all of the page's images are saved in the same directory.
    //      For the moment, we're not allowing this option.

    memset(&SaveFileName, 0, sizeof(SaveFileName));
    SaveFileName.lStructSize = sizeof(SaveFileName);
    SaveFileName.hwndOwner = m_hWnd;
    SaveFileName.hInstance = NULL;
    SaveFileName.lpstrFilter = "Web Page, HTML Only (*.htm;*.html)\0*.htm;*.html\0Text File (*.txt)\0*.txt\0"; 
    SaveFileName.lpstrCustomFilter = NULL; 
    SaveFileName.nMaxCustFilter = NULL; 
    SaveFileName.nFilterIndex = 1; 
    SaveFileName.lpstrFile = szFile; 
    SaveFileName.nMaxFile = sizeof(szFile); 
    SaveFileName.lpstrFileTitle = szFileTitle;
    SaveFileName.nMaxFileTitle = sizeof(szFileTitle); 
    SaveFileName.lpstrInitialDir = NULL; 
    SaveFileName.lpstrTitle = NULL; 
    SaveFileName.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT; 
    SaveFileName.nFileOffset = NULL; 
    SaveFileName.nFileExtension = NULL; 
    SaveFileName.lpstrDefExt = "htm"; 
    SaveFileName.lCustData = NULL; 
    SaveFileName.lpfnHook = NULL; 
    SaveFileName.lpTemplateName = NULL; 


    //Get the title of the current web page to set as the default filename.
    USES_CONVERSION;
    char szTmp[256]; // XXX hardcoded URLs array lengths are BAD
    memset(szTmp, 0, sizeof(szTmp));

    get_LocationName(&pageName);
    strcpy(szTmp, OLE2A(pageName));
    
    // The SaveAs dialog will fail if szFile contains any "bad" characters.
    // This hunk of code attempts to mimick the IE way of replacing "bad"
    // characters with "good" characters.
    int j = 0;
    for (int i=0; szTmp[i]!='\0'; i++)
    {        
        switch(szTmp[i])
        {
        case '\\':
        case '*':
        case '|':
        case ':':
        case '"':
        case '>':
        case '<':
        case '?':
            break;
        case '.':
            if (szTmp[i+1] != '\0')
            {
                szFile[j] = '_';
                j++;
            }
            break;
        case '/':
            szFile[j] = '-';
            j++;
            break;
        default:
            szFile[j] = szTmp[i];
            j++;
        }
    }
    szFile[j] = '\0';

    HRESULT hr = S_OK;
    if (GetSaveFileName(&SaveFileName))
    {
        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(mWebBrowser));
        USES_CONVERSION;

        char szDataFile[_MAX_PATH];
        char szDataPath[_MAX_PATH];
        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        char fname[_MAX_FNAME];
        char ext[_MAX_EXT];

        _splitpath(szFile, drive, dir, fname, ext);
        sprintf(szDataFile, "%s_files", fname);
        _makepath(szDataPath, drive, dir, szDataFile, "");

        nsCOMPtr<nsILocalFile> file;
        NS_NewLocalFile(T2A(szFile), TRUE, getter_AddRefs(file));

        nsCOMPtr<nsILocalFile> dataPath;
        NS_NewLocalFile(szDataPath, TRUE, getter_AddRefs(dataPath));

        persist->SaveDocument(nsnull, file, dataPath);
    }

    return hr;
}

LRESULT CMozillaBrowser::OnProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnProperties);
    MessageBox(_T("No Properties Yet!"), _T("Control Message"), MB_OK);
    // TODO show the properties dialog
    return 0;
}

LRESULT CMozillaBrowser::OnCut(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnCut);
    nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWebBrowser));
    if (clipboard)
    {
        clipboard->CutSelection();
    }
    return 0;
}

LRESULT CMozillaBrowser::OnCopy(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnCopy);
    nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWebBrowser));
    if (clipboard)
    {
        clipboard->CopySelection();
    }
    return 0;
}

LRESULT CMozillaBrowser::OnPaste(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnPaste);
    nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWebBrowser));
    if (clipboard)
    {
        clipboard->Paste();
    }
    return 0;
}

LRESULT CMozillaBrowser::OnSelectAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnSelectAll);
    nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWebBrowser));
    if (clipboard)
    {
        clipboard->SelectAll();
    }
    return 0;
}

// Handle ID_VIEWSOURCE command
LRESULT CMozillaBrowser::OnViewSource(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnViewSource);

    if (mWebBrowserContainer->m_pCurrentURI)
    {
        // No URI to view!
        NG_ASSERT(0);
        return 0;
    }

    // Get the current URI
    nsXPIDLCString aURI;
    mWebBrowserContainer->m_pCurrentURI->GetSpec(getter_Copies(aURI));

    nsAutoString strURI;
    strURI.AssignWithConversion("view-source:");
    strURI.AppendWithConversion(aURI);

    CIPtr(IDispatch) spDispNew;
    VARIANT_BOOL bCancel = VARIANT_FALSE;
    Fire_NewWindow2(&spDispNew, &bCancel);

    if ((bCancel == VARIANT_FALSE) && spDispNew)
    {
        CIPtr(IWebBrowser2) spOther = spDispNew;;
        if (spOther)
        {
            // tack in the viewsource command
            CComBSTR bstrURL(strURI.get());
            CComVariant vURL(bstrURL);
            VARIANT vNull;
            vNull.vt = VT_NULL;
            spOther->Navigate2(&vURL, &vNull, &vNull, &vNull, &vNull);            
        }
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Document handlers


LRESULT CMozillaBrowser::OnDocumentBack(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    GoBack();
    return 0;
}


LRESULT CMozillaBrowser::OnDocumentForward(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    GoForward();
    return 0;
}


LRESULT CMozillaBrowser::OnDocumentSelectAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    return OnSelectAll(wNotifyCode, wID, hWndCtl, bHandled);
}


LRESULT CMozillaBrowser::OnDocumentPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    return OnPrint(wNotifyCode, wID, hWndCtl, bHandled);
}


LRESULT CMozillaBrowser::OnDocumentRefresh(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    Refresh();
    return 0;
}


LRESULT CMozillaBrowser::OnDocumentProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    nsCOMPtr<nsIDOMDocument> ownerDoc;
    if (mContextNode)
    {
        mContextNode->GetOwnerDocument(getter_AddRefs(ownerDoc));
    }

    // Get the document URL
    nsAutoString uri;
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(ownerDoc);
    if (htmlDoc)
    {
        htmlDoc->GetURL(uri);
    }

    ShowURIPropertyDlg(uri);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Link handlers


LRESULT CMozillaBrowser::OnLinkOpen(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    nsAutoString uri;

    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement = do_QueryInterface(mContextNode);
    if (anchorElement)
    {
        anchorElement->GetHref(uri);
    }

    if (uri.Length() > 0)
    {
        CComBSTR bstrURI(uri.get());
        CComVariant vFlags(0);
        Navigate(bstrURI, &vFlags, NULL, NULL, NULL);
    }

    return 0;
}


LRESULT CMozillaBrowser::OnLinkOpenInNewWindow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    nsAutoString uri;

    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement = do_QueryInterface(mContextNode);
    if (anchorElement)
    {
        anchorElement->GetHref(uri);
    }

    if (uri.Length() > 0)
    {
        CComBSTR bstrURI(uri.get());
        CComVariant vFlags(navOpenInNewWindow);
        Navigate(bstrURI, &vFlags, NULL, NULL, NULL);
    }

    return 0;
}


LRESULT CMozillaBrowser::OnLinkCopyShortcut(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    nsAutoString uri;

    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement = do_QueryInterface(mContextNode);
    if (anchorElement)
    {
        anchorElement->GetHref(uri);
    }

    if (uri.Length() > 0 && OpenClipboard())
    {
        EmptyClipboard();

        // CF_TEXT
        HGLOBAL hmemText = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, uri.Length() + 1);
        char *pszText = (char *) GlobalLock(hmemText);
        uri.ToCString(pszText, uri.Length() + 1);
        GlobalUnlock(hmemText);
        SetClipboardData(CF_TEXT, hmemText);

        // UniformResourceLocator - CFSTR_SHELLURL
        const UINT cfShellURL = RegisterClipboardFormat(CFSTR_SHELLURL);
        HGLOBAL hmemURL = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, uri.Length() + 1);
        char *pszURL = (char *) GlobalLock(hmemURL);
        uri.ToCString(pszURL, uri.Length() + 1);
        GlobalUnlock(hmemURL);
        SetClipboardData(cfShellURL, hmemURL);

        // TODO
        // FileContents - CFSTR_FILECONTENTS
        // FileGroupDescriptor - CFSTR_FILEDESCRIPTORA
        // FileGroupDescriptorW - CFSTR_FILEDESCRIPTORW

        CloseClipboard();
    }
    return 0;
}


LRESULT CMozillaBrowser::OnLinkProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    nsAutoString uri;

    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement = do_QueryInterface(mContextNode);
    if (anchorElement)
    {
        anchorElement->GetHref(uri);
    }

    ShowURIPropertyDlg(uri);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////


// Test if the browser is in a valid state
BOOL CMozillaBrowser::IsValid()
{
    NG_TRACE_METHOD(CMozillaBrowser::IsValid);
    return mValidBrowserFlag;
}


// Initialises the web shell engine
HRESULT CMozillaBrowser::Initialize()
{
#ifdef HACK_NON_REENTRANCY
    // Attempt to open a named event for this process. If it's not there we
    // know this is the first time the control has run in this process, so create
    // the named event and do the initialisation. Otherwise do nothing.
    TCHAR szHackEvent[255];
    _stprintf(szHackEvent, _T("MozCtlEvent%d"), (int) GetCurrentProcessId());
    s_hHackedNonReentrancy = OpenEvent(EVENT_ALL_ACCESS, FALSE, szHackEvent);
    if (s_hHackedNonReentrancy == NULL)
    {
        s_hHackedNonReentrancy = CreateEvent(NULL, FALSE, FALSE, szHackEvent);
#endif

    // Extract the bin directory path from the control's filename
    TCHAR szMozCtlPath[MAX_PATH];
    memset(szMozCtlPath, 0, sizeof(szMozCtlPath));
    GetModuleFileName(_Module.m_hInst, szMozCtlPath, sizeof(szMozCtlPath) / sizeof(szMozCtlPath[0]));

    TCHAR szTmpDrive[_MAX_DRIVE];
    TCHAR szTmpDir[_MAX_DIR];
    TCHAR szTmpFname[_MAX_FNAME];
    TCHAR szTmpExt[_MAX_EXT];
    TCHAR szBinDirPath[MAX_PATH];

    _tsplitpath(szMozCtlPath, szTmpDrive, szTmpDir, szTmpFname, szTmpExt);
    memset(szBinDirPath, 0, sizeof(szBinDirPath));
    _tmakepath(szBinDirPath, szTmpDrive, szTmpDir, NULL, NULL);
    if (_tcslen(szBinDirPath) == 0)
    {
        return E_FAIL;
    }

    nsresult rv;

    // Create a directory file loc object
    nsCOMPtr<nsIDirectoryServiceProvider> provider; // nsnull for the moment
    // TODO put alternative directory service provider here at some point

    // Create an object to represent the path
    nsCOMPtr<nsILocalFile> binDir;
    USES_CONVERSION;
    NS_NewLocalFile(T2A(szBinDirPath), TRUE, getter_AddRefs(binDir));
    rv = NS_InitEmbedding(binDir, provider);

    // Load preferences service
    rv = nsServiceManager::GetService(kPrefCID, 
                                    NS_GET_IID(nsIPref), 
                                    (nsISupports **)&mPrefs);
    if (NS_FAILED(rv))
    {
        NG_ASSERT(0);
        NG_TRACE_ALWAYS(_T("Could not create preference object rv=%08x\n"), (int) rv);
        SetStartupErrorMessage(IDS_CANNOTCREATEPREFS);
        return E_FAIL;
    }

    // Set the profile which the control will use
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
        return E_FAIL;
    }

    // Register our own native prompting service for message boxes, login
    // prompts etc.

    nsCOMPtr<nsIFactory> promptFactory;
    rv = NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));
    if (NS_FAILED(rv)) return rv;
    rv = nsComponentManager::RegisterFactory(kPromptServiceCID,
        "Prompt Service",
        "@mozilla.org/embedcomp/prompt-service;1",
        promptFactory,
        PR_TRUE); // replace existing

    // Make a new default profile
    nsAutoString newProfileName; newProfileName.AssignWithConversion("MozillaControl");
    PRBool profileExists = PR_FALSE;
    rv = profileService->ProfileExists(newProfileName.get(), &profileExists);
    if (NS_FAILED(rv))
    {
        return E_FAIL;
    }
    else if (!profileExists)
    {
        rv = profileService->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_FALSE);
        if (NS_FAILED(rv))
        {
            return E_FAIL;
        }
    }
    rv = profileService->SetCurrentProfile(newProfileName.get());
    if (NS_FAILED(rv))
    {
        return E_FAIL;
    }

#ifdef HACK_NON_REENTRANCY
    }
#endif

    return S_OK;
}

// Terminates the web shell engine
HRESULT CMozillaBrowser::Terminate()
{
#ifdef HACK_NON_REENTRANCY
    if (0)
    {
#endif

    NS_IF_RELEASE(mPrefs);
    NS_TermEmbedding();

#ifdef HACK_NON_REENTRANCY
    }
#endif

    return S_OK;
}

// Create and initialise the web shell
HRESULT CMozillaBrowser::CreateBrowser() 
{    
    NG_TRACE_METHOD(CMozillaBrowser::CreateBrowser);
    
    if (mWebBrowser != nsnull)
    {
        NG_ASSERT(0);
        NG_TRACE_ALWAYS(_T("CreateBrowser() called more than once!"));
        RETURN_E_UNEXPECTED();
    }

    RECT rcLocation;
    GetClientRect(&rcLocation);
    if (IsRectEmpty(&rcLocation))
    {
        rcLocation.bottom++;
        rcLocation.top++;
    }

    nsresult rv;

    PRBool aAllowPlugins = PR_TRUE;

    // Create the web browser
    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
        NG_ASSERT(0);
        NG_TRACE_ALWAYS(_T("Could not create webbrowser object rv=%08x\n"), (int) rv);
        SetStartupErrorMessage(IDS_CANNOTCREATEPREFS);
        return rv;
    }

    // Configure what the web browser can and cannot do
    nsCOMPtr<nsIWebBrowserSetup> webBrowserAsSetup(do_QueryInterface(mWebBrowser));
    // webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, aAllowPlugins);
    // webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_CONTAINS_CHROME, PR_TRUE);

    // Create the webbrowser window
    mWebBrowserAsWin = do_QueryInterface(mWebBrowser);
    rv = mWebBrowserAsWin->InitWindow(nsNativeWidget(m_hWnd), nsnull,
        0, 0, rcLocation.right - rcLocation.left, rcLocation.bottom - rcLocation.top);
    rv = mWebBrowserAsWin->Create();

    // Create the container object
    mWebBrowserContainer = new CWebBrowserContainer(this);
    if (mWebBrowserContainer == NULL)
    {
        NG_ASSERT(0);
        NG_TRACE_ALWAYS(_T("Could not create webbrowsercontainer - out of memory\n"));
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mWebBrowserContainer->AddRef();

    // Set up the browser with its chrome
    mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, mWebBrowserContainer));
    mWebBrowser->SetParentURIContentListener(mWebBrowserContainer);

    // Subscribe for progress notifications
    nsCOMPtr<nsIWeakReference> listener(
        dont_AddRef(NS_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, mWebBrowserContainer))));
    mWebBrowser->AddWebBrowserListener(listener, NS_GET_IID(nsIWebProgressListener));

    // Visible
    mWebBrowserAsWin->SetVisibility(PR_TRUE);

    // Append browser to browser list
    sBrowserList.AppendElement(this);

    mValidBrowserFlag = TRUE;

    return S_OK;
}

// Clean up the browser
HRESULT CMozillaBrowser::DestroyBrowser()
{
    // TODO unregister drop target

    mValidBrowserFlag = FALSE;

    // Remove browser from browser list
    sBrowserList.RemoveElement(this);

     // Destroy the htmldoc
     if (mIERootDocument != NULL)
     {
         mIERootDocument->Release();
         mIERootDocument = NULL;
     }

    // Destroy layout...
    if (mWebBrowserAsWin)
    {
        mWebBrowserAsWin->Destroy();
        mWebBrowserAsWin = nsnull;
    }

    if (mWebBrowserContainer)
    {
        mWebBrowserContainer->Release();
        mWebBrowserContainer = NULL;
    }

    mWebBrowser = nsnull;

    return S_OK;
}


// Turns the editor mode on or off
HRESULT CMozillaBrowser::SetEditorMode(BOOL bEnabled)
{
    NG_TRACE_METHOD(CMozillaBrowser::SetEditorMode);
    return S_OK;
}


HRESULT CMozillaBrowser::OnEditorCommand(DWORD nCmdID)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnEditorCommand);

    static nsIAtom * propB = NS_NewAtom("b");       
    static nsIAtom * propI = NS_NewAtom("i");     
    static nsIAtom * propU = NS_NewAtom("u");     

    if (!mEditModeFlag)
    {
        return E_UNEXPECTED;
    }
    if (!mEditor)
    {
        NG_ASSERT(0);
        return E_UNEXPECTED;
    }

    nsCOMPtr<nsIHTMLEditor> pHtmlEditor = do_QueryInterface(mEditor);

    bool bToggleInlineProperty = false;
    nsIAtom *pInlineProperty = nsnull;

    switch (nCmdID)
    {
    case IDM_BOLD:
        pInlineProperty = propB;
        bToggleInlineProperty = true;
        break;
    case IDM_ITALIC:
        pInlineProperty = propI;
        bToggleInlineProperty = true;
        break;
    case IDM_UNDERLINE:
        pInlineProperty = propU;
        bToggleInlineProperty = true;
        break;
    
    // TODO add the rest!
    
    default:
        // DO NOTHING
        break;
    }

    // Does the instruction involve toggling something? e.g. B, U, I
    if (bToggleInlineProperty)
    {
        PRBool bFirst = PR_TRUE;
        PRBool bAny = PR_TRUE;
        PRBool bAll = PR_TRUE;

        // Set or remove
        pHtmlEditor->GetInlineProperty(pInlineProperty, nsString(), nsString(), &bFirst, &bAny, &bAll);
        if (bAny)
        {
            pHtmlEditor->RemoveInlineProperty(pInlineProperty, nsString());
        }
        else
        {
            pHtmlEditor->SetInlineProperty(pInlineProperty, nsString(), nsString());
        }
    }

    return S_OK;
}


// Return the root DOM document
HRESULT CMozillaBrowser::GetDOMDocument(nsIDOMDocument **pDocument)
{
    NG_TRACE_METHOD(CMozillaBrowser::GetDOMDocument);

    HRESULT hr = E_FAIL;

    // Test for stupid args
    if (pDocument == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    *pDocument = nsnull;

    if (!IsValid())
    {
        NG_ASSERT(0);
        return E_UNEXPECTED;
    }

    // Get the DOM window from the webbrowser
    nsCOMPtr<nsIDOMWindow> window;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(window));
    if (window)
    {
        if (NS_SUCCEEDED(window->GetDocument(pDocument)) && *pDocument)
        {
            hr = S_OK;
        }
    }

    return hr;
}


// Load any browser helpers
HRESULT CMozillaBrowser::LoadBrowserHelpers()
{
    NG_TRACE_METHOD(CMozillaBrowser::LoadBrowserHelpers);

    UnloadBrowserHelpers();

    // IE loads browser helper objects from a branch of the registry
    // Search the branch looking for objects to load with the control.

    CRegKey cKey;
    if (cKey.Open(HKEY_LOCAL_MACHINE, c_szIEHelperObjectKey, KEY_ENUMERATE_SUB_KEYS) != ERROR_SUCCESS)
    {
        NG_TRACE(_T("No browser helper key found\n"));
        return S_FALSE;
    }

    // Count the number of browser helper object keys
    ULONG nHelperKeys = 0;
    LONG nResult = ERROR_SUCCESS;
    while (nResult == ERROR_SUCCESS)
    {
        TCHAR szCLSID[50];
        DWORD dwCLSID = sizeof(szCLSID) / sizeof(TCHAR);
        FILETIME cLastWrite;
        
        // Read next subkey
        nResult = RegEnumKeyEx(cKey, nHelperKeys, szCLSID, &dwCLSID, NULL, NULL, NULL, &cLastWrite);
        if (nResult != ERROR_SUCCESS)
        {
            break;
        }
        nHelperKeys++;
    }
    if (nHelperKeys == 0)
    {
         NG_TRACE(_T("No browser helper objects found\n"));
        return S_FALSE;
    }

    // Get the CLSID for each browser helper object
    CLSID *pClassList = new CLSID[nHelperKeys];
    DWORD nHelpers = 0;
    DWORD nKey = 0;
    nResult = ERROR_SUCCESS;
    while (nResult == ERROR_SUCCESS)
    {
        TCHAR szCLSID[50];
        DWORD dwCLSID = sizeof(szCLSID) / sizeof(TCHAR);
        FILETIME cLastWrite;
        
        // Read next subkey
        nResult = RegEnumKeyEx(cKey, nKey++, szCLSID, &dwCLSID, NULL, NULL, NULL, &cLastWrite);
        if (nResult != ERROR_SUCCESS)
        {
            break;
        }

        NG_TRACE(_T("Reading helper object \"%s\"\n"), szCLSID);

        // Turn the key into a CLSID
        USES_CONVERSION;
        CLSID clsid;
        if (CLSIDFromString(T2OLE(szCLSID), &clsid) != NOERROR)
        {
            continue;
        }

        pClassList[nHelpers++] = clsid;
    }

    mBrowserHelperList = new CComUnkPtr[nHelpers];

    // Create each object in turn
    for (ULONG i = 0; i < nHelpers; i++)
    {
        CLSID clsid = pClassList[i];
        HRESULT hr;
        CComQIPtr<IObjectWithSite, &IID_IObjectWithSite> cpObjectWithSite;

        hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IObjectWithSite, (LPVOID *) &cpObjectWithSite);
        if (FAILED(hr))
        {
            NG_TRACE(_T("Registered browser helper object cannot be created\n"));;
        }

        // Set the object to point at the browser
        cpObjectWithSite->SetSite((IWebBrowser2 *) this);

        // Store in the list
        CComUnkPtr cpUnk = cpObjectWithSite;
        mBrowserHelperList[mBrowserHelperListCount++] = cpUnk;
    }

    delete []pClassList;
        
    return S_OK;
}

// Release browser helpers
HRESULT CMozillaBrowser::UnloadBrowserHelpers()
{
    NG_TRACE_METHOD(CMozillaBrowser::UnloadBrowserHelpers);

    if (mBrowserHelperList == NULL)
    {
        return S_OK;
    }

    // Destroy each helper    
    for (ULONG i = 0; i < mBrowserHelperListCount; i++)
    {
        CComUnkPtr cpUnk = mBrowserHelperList[i];
        if (cpUnk)
        {
            CComQIPtr<IObjectWithSite, &IID_IObjectWithSite> cpObjectWithSite = cpUnk;
            if (cpObjectWithSite)
            {
                cpObjectWithSite->SetSite(NULL);
            }
        }
    }

    // Cleanup the array
    mBrowserHelperListCount = 0;
    delete []mBrowserHelperList;
    mBrowserHelperList = NULL;

    return S_OK;
}

// Print document
HRESULT CMozillaBrowser::PrintDocument(BOOL promptUser)
{
    nsresult rv;

    PRBool oldPrintSilent = PR_FALSE;
    nsCOMPtr<nsIPrintOptions> printService = 
             do_GetService(kPrintOptionsCID, &rv);
    if (printService)
    {
        printService->GetPrintSilent(&oldPrintSilent);
        printService->SetPrintSilent(promptUser ? PR_FALSE : PR_TRUE);
    }

    // Print the contents
    nsCOMPtr<nsIWebBrowserPrint> browserAsPrint = do_QueryInterface(mWebBrowser);
    nsCOMPtr<nsIDOMWindow> window;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(window));
    if (window)
    {
        PrintListener *listener = new PrintListener;
        nsCOMPtr<nsIPrintListener> printListener = do_QueryInterface(listener);
        browserAsPrint->Print(window, nsnull, printListener);
        listener->WaitForComplete();
    }

    if (printService)
    {
        printService->SetPrintSilent(oldPrintSilent);
    }
    
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// IOleObject overrides

// This is an almost verbatim copy of the standard ATL implementation of
// IOleObject::InPlaceActivate but with a few lines commented out.

HRESULT CMozillaBrowser::InPlaceActivate(LONG iVerb, const RECT* prcPosRect)
{
    NG_TRACE_METHOD(CMozillaBrowser::InPlaceActivate);

    HRESULT hr;

    if (m_spClientSite == NULL)
        return S_OK;

    CComPtr<IOleInPlaceObject> pIPO;
    ControlQueryInterface(IID_IOleInPlaceObject, (void**)&pIPO);
    _ASSERTE(pIPO != NULL);
    if (prcPosRect != NULL)
        pIPO->SetObjectRects(prcPosRect, prcPosRect);

    if (!m_bNegotiatedWnd)
    {
        if (!m_bWindowOnly)
            // Try for windowless site
            hr = m_spClientSite->QueryInterface(IID_IOleInPlaceSiteWindowless, (void **)&m_spInPlaceSite);

        if (m_spInPlaceSite)
        {
            m_bInPlaceSiteEx = TRUE;
            m_bWndLess = SUCCEEDED(m_spInPlaceSite->CanWindowlessActivate());
            m_bWasOnceWindowless = TRUE;
        }
        else
        {
            m_spClientSite->QueryInterface(IID_IOleInPlaceSiteEx, (void **)&m_spInPlaceSite);
            if (m_spInPlaceSite)
                m_bInPlaceSiteEx = TRUE;
            else
                hr = m_spClientSite->QueryInterface(IID_IOleInPlaceSite, (void **)&m_spInPlaceSite);
        }
    }

    _ASSERTE(m_spInPlaceSite);
    if (!m_spInPlaceSite)
        return E_FAIL;

    m_bNegotiatedWnd = TRUE;

    if (!m_bInPlaceActive)
    {

        BOOL bNoRedraw = FALSE;
        if (m_bWndLess)
            m_spInPlaceSite->OnInPlaceActivateEx(&bNoRedraw, ACTIVATE_WINDOWLESS);
        else
        {
            if (m_bInPlaceSiteEx)
                m_spInPlaceSite->OnInPlaceActivateEx(&bNoRedraw, 0);
            else
            {
                HRESULT hr = m_spInPlaceSite->CanInPlaceActivate();
                if (FAILED(hr))
                    return hr;
                m_spInPlaceSite->OnInPlaceActivate();
            }
        }
    }

    m_bInPlaceActive = TRUE;

    // get location in the parent window,
    // as well as some information about the parent
    //
    OLEINPLACEFRAMEINFO frameInfo;
    RECT rcPos, rcClip;
    CComPtr<IOleInPlaceFrame> spInPlaceFrame;
    CComPtr<IOleInPlaceUIWindow> spInPlaceUIWindow;
    frameInfo.cb = sizeof(OLEINPLACEFRAMEINFO);
    HWND hwndParent;
    if (m_spInPlaceSite->GetWindow(&hwndParent) == S_OK)
    {
        m_spInPlaceSite->GetWindowContext(&spInPlaceFrame,
            &spInPlaceUIWindow, &rcPos, &rcClip, &frameInfo);

        if (!m_bWndLess)
        {
            if (m_hWndCD)
            {
                ::ShowWindow(m_hWndCD, SW_SHOW);
                ::SetFocus(m_hWndCD);
            }
            else
            {
                HWND h = CreateControlWindow(hwndParent, rcPos);
                _ASSERTE(h == m_hWndCD);
            }
        }

        pIPO->SetObjectRects(&rcPos, &rcClip);
    }

    CComPtr<IOleInPlaceActiveObject> spActiveObject;
    ControlQueryInterface(IID_IOleInPlaceActiveObject, (void**)&spActiveObject);

    // Gone active by now, take care of UIACTIVATE
    if (DoesVerbUIActivate(iVerb))
    {
        if (!m_bUIActive)
        {
            m_bUIActive = TRUE;
            hr = m_spInPlaceSite->OnUIActivate();
            if (FAILED(hr))
                return hr;

            SetControlFocus(TRUE);
            // set ourselves up in the host.
            //
            if (spActiveObject)
            {
                if (spInPlaceFrame)
                    spInPlaceFrame->SetActiveObject(spActiveObject, NULL);
                if (spInPlaceUIWindow)
                    spInPlaceUIWindow->SetActiveObject(spActiveObject, NULL);
            }

// These lines are deliberately commented out to demonstrate what breaks certain
// control containers.

//            if (spInPlaceFrame)
//                spInPlaceFrame->SetBorderSpace(NULL);
//            if (spInPlaceUIWindow)
//                spInPlaceUIWindow->SetBorderSpace(NULL);
        }
    }

    m_spClientSite->ShowObject();

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GetClientSite(IOleClientSite **ppClientSite)
{
    NG_TRACE_METHOD(CMozillaBrowser::GetClientSite);

    NG_ASSERT(ppClientSite);

    // This fixes a problem in the base class which asserts if the client
    // site has not been set when this method is called. 

    HRESULT hRes = E_POINTER;
    if (ppClientSite != NULL)
    {
        *ppClientSite = NULL;
        if (m_spClientSite == NULL)
        {
            return S_OK;
        }
    }

    return IOleObjectImpl<CMozillaBrowser>::GetClientSite(ppClientSite);
}


/////////////////////////////////////////////////////////////////////////////
// IMozControlBridge


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GetWebBrowser(/* [out] */ void __RPC_FAR *__RPC_FAR *aBrowser)
{
    if (!NgIsValidAddress(aBrowser, sizeof(void *)))
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    *aBrowser = NULL;
    if (mWebBrowser)
    {
        nsIWebBrowser *browser = mWebBrowser.get();
        NS_ADDREF(browser);
        *aBrowser = (void *) browser;
    }
    return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// IWebBrowser

HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoBack(void)
{
    NG_TRACE_METHOD(CMozillaBrowser::GoBack);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    PRBool aCanGoBack = PR_FALSE;
    webNav->GetCanGoBack(&aCanGoBack);
    if (aCanGoBack == PR_TRUE)
    {
        webNav->GoBack();
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoForward(void)
{
    NG_TRACE_METHOD(CMozillaBrowser::GoForward);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    PRBool aCanGoForward = PR_FALSE;
    webNav->GetCanGoForward(&aCanGoForward);
    if (aCanGoForward == PR_TRUE)
    {
        webNav->GoForward();
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoHome(void)
{
    NG_TRACE_METHOD(CMozillaBrowser::GoHome);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    nsAutoString sUrl;
    sUrl.AssignWithConversion("http://home.netscape.com");

    // Find the home page stored in prefs
    if (mPrefs)
    {
        nsXPIDLString szBuffer;
        nsresult rv;
        rv = mPrefs->GetLocalizedUnicharPref(c_szPrefsHomePage, getter_Copies(szBuffer));
        if (rv == NS_OK)
        {
            sUrl.Assign(szBuffer);
        }
    }
    // Navigate to the home page
    CComBSTR bstrUrl(sUrl.get());
    Navigate(bstrUrl , NULL, NULL, NULL, NULL);
    
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GoSearch(void)
{
    NG_TRACE_METHOD(CMozillaBrowser::GoSearch);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    OLECHAR * sUrl = L"http://search.netscape.com";

    // Find the home page stored in prefs
    if (mPrefs)
    {
        // TODO find and navigate to the search page stored in prefs
        //      and not this hard coded address
    }
    Navigate(sUrl, NULL, NULL, NULL, NULL);
    
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate(BSTR URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
    NG_TRACE_METHOD(CMozillaBrowser::Navigate);

    //Make sure browser is valid
    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    // Extract the URL parameter
    if (URL == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    nsAutoString sUrl(URL);

    // Check for a view-source op - this is a bit kludgy
    // TODO
    if (sUrl.CompareWithConversion(L"view-source:", PR_TRUE, 12) == 0)
     {
        // Broken code - appears to want to replace view-source: with view: to 
        // get Mozilla to respond to the IE view-source: protocol.
///        std::wstring sCommand(L"view");
//        sUrl.Left(sCommand, 11);
//         sUrl.Cut(0,12);
     }

    // Extract the launch flags parameter
    LONG lFlags = 0;
    if (Flags &&
        Flags->vt != VT_ERROR &&
        Flags->vt != VT_EMPTY &&
        Flags->vt != VT_NULL)
    {
        CComVariant vFlags;
        if ( vFlags.ChangeType(VT_I4, Flags) != S_OK )
        {
            NG_ASSERT(0);
            RETURN_E_INVALIDARG();
        }
        lFlags = vFlags.lVal;
    }

    // TODO Extract the headers parameter
    PRBool bModifyHistory = PR_TRUE;

    // Check the navigation flags
    if (lFlags & navOpenInNewWindow)
    {
        CIPtr(IDispatch) spDispNew;
        VARIANT_BOOL bCancel = VARIANT_FALSE;
        
        // Test if the event sink can give us a new window to navigate into
        Fire_NewWindow2(&spDispNew, &bCancel);

        lFlags &= ~(navOpenInNewWindow);
        if ((bCancel == VARIANT_FALSE) && spDispNew)
        {
            CIPtr(IWebBrowser2) spOther = spDispNew;
            if (spOther)
            {
                CComVariant vURL(URL);
                CComVariant vFlags(lFlags);
                return spOther->Navigate2(&vURL, &vFlags, TargetFrameName, PostData, Headers);
            }
        }
        // Can't open a new window without client support
        return E_NOTIMPL;
    }
    // Extract the target frame parameter
    std::wstring sTargetFrame;
    if (TargetFrameName && TargetFrameName->vt == VT_BSTR)
    {
        sTargetFrame = TargetFrameName->bstrVal;
    }

    // Extract the post data parameter
    mLastPostData.Clear();
    if (PostData)
    {
        mLastPostData.Copy(PostData);
    }

    if (PostData && PostData->vt == VT_BSTR)
    {
        USES_CONVERSION;
        char *szPostData = OLE2A(PostData->bstrVal);
    }
    if (lFlags & navNoHistory)
    {
        // Disable history
        bModifyHistory = PR_FALSE;
    }
    if (lFlags & navNoReadFromCache)
    {
        // TODO disable read from cache
    }
    if (lFlags & navNoWriteToCache)
    {
        // TODO disable write to cache
    }

    // TODO find the correct target frame

    // Load the URL    
    nsresult res = NS_ERROR_FAILURE;

    nsCOMPtr<nsIWebNavigation> spIWebNavigation = do_QueryInterface(mWebBrowser);
    if (spIWebNavigation)
    {
        res = spIWebNavigation->LoadURI(sUrl.get(), nsIWebNavigation::LOAD_FLAGS_NONE);
    }

    return res;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh(void)
{
    NG_TRACE_METHOD(CMozillaBrowser::Refresh);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    // Reload the page
    CComVariant vRefreshType(REFRESH_NORMAL);
    return Refresh2(&vRefreshType);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Refresh2(VARIANT __RPC_FAR *Level)
{
    NG_TRACE_METHOD(CMozillaBrowser::Refresh2);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_NULL_OR_POINTER(Level, VARIANT);

    // Check the requested refresh type
    OLECMDID_REFRESHFLAG iRefreshLevel = OLECMDIDF_REFRESH_NORMAL;
    if (Level)
    {
        CComVariant vLevelAsInt;
        if ( vLevelAsInt.ChangeType(VT_I4, Level) != S_OK )
        {
            NG_ASSERT(0);
            RETURN_E_UNEXPECTED();
        }
        iRefreshLevel = (OLECMDID_REFRESHFLAG) vLevelAsInt.iVal;
    }

    // Turn the IE refresh type into the nearest NG equivalent
    PRUint32 flags = nsIWebNavigation::LOAD_FLAGS_NONE;
    switch (iRefreshLevel & OLECMDIDF_REFRESH_LEVELMASK)
    {
    case OLECMDIDF_REFRESH_NORMAL:
    case OLECMDIDF_REFRESH_IFEXPIRED:
    case OLECMDIDF_REFRESH_CONTINUE:
    case OLECMDIDF_REFRESH_NO_CACHE:
    case OLECMDIDF_REFRESH_RELOAD:
        flags = nsIWebNavigation::LOAD_FLAGS_NONE;
        break;
    case OLECMDIDF_REFRESH_COMPLETELY:
        flags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
        break;
    default:
        // No idea what refresh type this is supposed to be
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    nsCOMPtr<nsIWebNavigation> spIWebNavigation = do_QueryInterface(mWebBrowser);
    if (spIWebNavigation)
    {
        spIWebNavigation->Reload(flags);
    }
    
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::Stop()
{
    NG_TRACE_METHOD(CMozillaBrowser::Stop);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    nsCOMPtr<nsIWebNavigation> spIWebNavigation = do_QueryInterface(mWebBrowser);
    if (spIWebNavigation)
    {
        spIWebNavigation->Stop(nsIWebNavigation::STOP_ALL);
    }
    
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Application(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Application);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    // Return a pointer to this controls dispatch interface
    *ppDisp = (IDispatch *) this;
    (*ppDisp)->AddRef();
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Parent);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    // Attempt to get the parent object of this control
    HRESULT hr = E_FAIL;
    if (m_spClientSite)
    {
        hr = m_spClientSite->QueryInterface(IID_IDispatch, (void **) ppDisp);
    }

    return (SUCCEEDED(hr)) ? S_OK : E_NOINTERFACE;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Container(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Container);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Container: Retrieve a pointer to the IDispatch interface of the container.
    *ppDisp = NULL;
    RETURN_E_UNEXPECTED();
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Document);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    *ppDisp = NULL;

    // Get hold of the DOM document
    nsIDOMDocument *pIDOMDocument = nsnull;
    if (FAILED(GetDOMDocument(&pIDOMDocument)) || pIDOMDocument == nsnull)
    {
        return S_OK; // return NULL pointer
    }

    if (mIERootDocument == NULL)
     {    
        nsCOMPtr <nsIDOMHTMLDocument> pIDOMHTMLDocument =
                do_QueryInterface(pIDOMDocument);

        if (!pIDOMDocument)
        {
            RETURN_ERROR(_T("get_Document: not HTML"), E_FAIL);
        }

         CIEHtmlDocumentInstance::CreateInstance(&mIERootDocument);
          if (mIERootDocument == NULL)
         {
            RETURN_ERROR(_T("get_Document: can't create IERootDocument"), E_OUTOFMEMORY);
         }
         
        // addref it so it doesn't go away on us.
         mIERootDocument->AddRef();
 
         // give it a pointer to us.  note that we shouldn't be addref'd by this call, or it would be 
         // a circular reference.
         mIERootDocument->SetParent(this);
        mIERootDocument->SetDOMNode(pIDOMDocument);
        mIERootDocument->SetNative(pIDOMHTMLDocument);
    }

    mIERootDocument->QueryInterface(IID_IDispatch, (void **) ppDisp);
    pIDOMDocument->Release();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TopLevelContainer(VARIANT_BOOL __RPC_FAR *pBool)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_TopLevelContainer);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (!NgIsValidAddress(pBool, sizeof(VARIANT_BOOL)))
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO:  Implement get_TopLevelContainer
    *pBool = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Type(BSTR __RPC_FAR *Type)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Type);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //NOTE:    This code should work in theory, but can't be verified because GetDoctype
    //        has not been implemented yet.
#if 0
    nsIDOMDocument *pIDOMDocument = nsnull;
    if ( SUCCEEDED(GetDOMDocument(&pIDOMDocument)) )
    {
        nsIDOMDocumentType *pIDOMDocumentType = nsnull;
        if ( SUCCEEDED(pIDOMDocument->GetDoctype(&pIDOMDocumentType)) )
        {
            nsAutoString docName;
            pIDOMDocumentType->GetName(docName);
            //NG_TRACE("pIDOMDocumentType returns: %s", docName);
            //Still need to manipulate docName so that it goes into *Type param of this function.
        }
    }
#endif

    //TODO: Implement get_Type
    RETURN_ERROR(_T("get_Type: failed"), E_FAIL);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Left(long __RPC_FAR *pl)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Left);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pl == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Left - Should return the left position of this control.
    *pl = 0;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Left(long Left)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Left);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }
    
    //TODO: Implement put_Left - Should set the left position of this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Top(long __RPC_FAR *pl)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Top);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pl == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Top - Should return the top position of this control.
    *pl = 0;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Top(long Top)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Top);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement set_Top - Should set the top position of this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Width(long __RPC_FAR *pl)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Width);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pl == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Width- Should return the width of this control.
    *pl = 0;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Width(long Width)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Width);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement put_Width - Should set the width of this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Height(long __RPC_FAR *pl)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Height);

    if (!IsValid())
    {
        RETURN_E_UNEXPECTED();
    }

    if (pl == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Height - Should return the hieght of this control.
    *pl = 0;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Height(long Height)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Height);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement put_Height - Should set the height of this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationName(BSTR __RPC_FAR *LocationName)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_LocationName);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (LocationName == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    // Get the url from the web shell
    nsXPIDLString szLocationName;
    mWebBrowserAsWin->GetTitle(getter_Copies(szLocationName));
    if (nsnull == (const PRUnichar *) szLocationName)
    {
        RETURN_E_UNEXPECTED();
    }

    // Convert the string to a BSTR
    USES_CONVERSION;
    LPCOLESTR pszConvertedLocationName = W2COLE((const PRUnichar *) szLocationName);
    *LocationName = SysAllocString(pszConvertedLocationName);

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_LocationURL(BSTR __RPC_FAR *LocationURL)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_LocationURL);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (LocationURL == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    nsCOMPtr<nsIURI> uri;

    // Get the current url from the browser
    nsCOMPtr<nsIWebNavigation> browserAsNav = do_QueryInterface(mWebBrowser);
    if (browserAsNav)
    {
        browserAsNav->GetCurrentURI(getter_AddRefs(uri));
    }

    if (uri)
    {
        USES_CONVERSION;
        nsXPIDLCString aURI;
        uri->GetSpec(getter_Copies(aURI));
        *LocationURL = SysAllocString(A2OLE((const char *) aURI));
    }
    else
    {
        *LocationURL = NULL;
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Busy(VARIANT_BOOL __RPC_FAR *pBool)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Busy);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (!NgIsValidAddress(pBool, sizeof(*pBool)))
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    *pBool = (mBusyFlag) ? VARIANT_TRUE : VARIANT_FALSE;

    return S_OK;
}


/////////////////////////////////////////////////////////////////////////////
// IWebBrowserApp

HRESULT STDMETHODCALLTYPE CMozillaBrowser::Quit(void)
{
    NG_TRACE_METHOD(CMozillaBrowser::Quit);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //This generates an exception in the IE control.
    // TODO fire quit event
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ClientToWindow(int __RPC_FAR *pcx, int __RPC_FAR *pcy)
{
    NG_TRACE_METHOD(CMozillaBrowser::ClientToWindow);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //This generates an exception in the IE control.
    // TODO convert points to be relative to browser
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::PutProperty(BSTR szProperty, VARIANT vtValue)
{
    NG_TRACE_METHOD(CMozillaBrowser::PutProperty);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (szProperty == NULL)
    {
        RETURN_E_INVALIDARG();
    }
    PropertyList::iterator i;
    for (i = mPropertyList.begin(); i != mPropertyList.end(); i++)
    {
        // Is the property already in the list?
        if (wcscmp((*i).szName, szProperty) == 0)
        {
            // Copy the new value
            (*i).vValue = CComVariant(vtValue);
            return S_OK;
        }
    }

    Property p;
    p.szName = CComBSTR(szProperty);
    p.vValue = vtValue;

    mPropertyList.push_back(p);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::GetProperty(BSTR Property, VARIANT __RPC_FAR *pvtValue)
{
    NG_TRACE_METHOD(CMozillaBrowser::GetProperty);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT(Property);
    NG_ASSERT_POINTER(pvtValue, VARIANT);
    
    if (Property == NULL || pvtValue == NULL)
    {
        RETURN_E_INVALIDARG();
    }
    
    VariantInit(pvtValue);
    PropertyList::iterator i;
    for (i = mPropertyList.begin(); i != mPropertyList.end(); i++)
    {
        // Is the property already in the list?
        if (wcscmp((*i).szName, Property) == 0)
        {
            // Copy the new value
            VariantCopy(pvtValue, &(*i).vValue);
            return S_OK;
        }
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Name(BSTR __RPC_FAR *Name)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Name);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(Name, BSTR);
    if (Name == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    // TODO: Implement get_Name (get Mozilla's executable name)
    *Name = SysAllocString(L"Mozilla Web Browser Control");
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_HWND(long __RPC_FAR *pHWND)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_HWND);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(pHWND, HWND);
    if (pHWND == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //This is supposed to return a handle to the IE main window.  Since that doesn't exist
    //in the control's case, this shouldn't do anything.
    *pHWND = NULL;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullName(BSTR __RPC_FAR *FullName)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_FullName);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(FullName, BSTR);
    if (FullName == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    // TODO: Implement get_FullName (Return the full path of the executable containing this control)
    *FullName = SysAllocString(L""); // TODO get Mozilla's executable name
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Path(BSTR __RPC_FAR *Path)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Path);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(Path, BSTR);
    if (Path == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    // TODO: Implement get_Path (get Mozilla's path)
    *Path = SysAllocString(L"");
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Visible(VARIANT_BOOL __RPC_FAR *pBool)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Visible);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(pBool, int);
    if (pBool == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Visible
    *pBool = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Visible(VARIANT_BOOL Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Visible);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement put_Visible
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusBar(VARIANT_BOOL __RPC_FAR *pBool)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_StatusBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(pBool, int);
    if (pBool == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //There is no StatusBar in this control.
    *pBool = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_StatusBar(VARIANT_BOOL Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_StatusBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //There is no StatusBar in this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_StatusText(BSTR __RPC_FAR *StatusText)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_StatusText);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(StatusText, BSTR);
    if (StatusText == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_StatusText
    //NOTE: This function is related to the MS status bar which doesn't exist in this control.  Needs more
    //        investigation, but probably doesn't apply.
    *StatusText = SysAllocString(L"");
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_StatusText(BSTR StatusText)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_StatusText);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement put_StatusText
    //NOTE: This function is related to the MS status bar which doesn't exist in this control.  Needs more
    //        investigation, but probably doesn't apply.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ToolBar(int __RPC_FAR *Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_ToolBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(Value, int);
    if (Value == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //There is no ToolBar in this control.
    *Value = FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_ToolBar(int Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_ToolBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //There is no ToolBar in this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_MenuBar(VARIANT_BOOL __RPC_FAR *Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_MenuBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(Value, int);
    if (Value == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //There is no MenuBar in this control.
    *Value = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_MenuBar(VARIANT_BOOL Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_MenuBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //There is no MenuBar in this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_FullScreen(VARIANT_BOOL __RPC_FAR *pbFullScreen)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_FullScreen);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    NG_ASSERT_POINTER(pbFullScreen, VARIANT_BOOL);
    if (pbFullScreen == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //FullScreen mode doesn't really apply to this control.
    *pbFullScreen = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_FullScreen(VARIANT_BOOL bFullScreen)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_FullScreen);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //FullScreen mode doesn't really apply to this control.
    return S_OK;
}



/////////////////////////////////////////////////////////////////////////////
// IWebBrowser2

HRESULT STDMETHODCALLTYPE CMozillaBrowser::Navigate2(VARIANT __RPC_FAR *URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
{
    NG_TRACE_METHOD(CMozillaBrowser::Navigate2);

    CComVariant vURLAsString;
    if ( vURLAsString.ChangeType(VT_BSTR, URL) != S_OK )
    {
        RETURN_E_INVALIDARG();
    }

    return Navigate(vURLAsString.bstrVal, Flags, TargetFrameName, PostData, Headers);
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::QueryStatusWB(OLECMDID cmdID, OLECMDF __RPC_FAR *pcmdf)
{
    NG_TRACE_METHOD(CMozillaBrowser::QueryStatusWB);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pcmdf == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    // Call through to IOleCommandTarget::QueryStatus
    OLECMD cmd;
    HRESULT hr;
    
    cmd.cmdID = cmdID;
    cmd.cmdf = 0;
    hr = QueryStatus(NULL, 1, &cmd, NULL);
    if (SUCCEEDED(hr))
    {
        *pcmdf = (OLECMDF) cmd.cmdf;
    }

    return hr;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ExecWB(OLECMDID cmdID, OLECMDEXECOPT cmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
{
    NG_TRACE_METHOD(CMozillaBrowser::ExecWB);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    // Call through to IOleCommandTarget::Exec
    HRESULT hr;
    hr = Exec(NULL, cmdID, cmdexecopt, pvaIn, pvaOut);
    return hr;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::ShowBrowserBar(VARIANT __RPC_FAR *pvaClsid, VARIANT __RPC_FAR *pvarShow, VARIANT __RPC_FAR *pvarSize)
{
    NG_TRACE_METHOD(CMozillaBrowser::ShowBrowserBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_ReadyState(READYSTATE __RPC_FAR *plReadyState)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_ReadyState);

    if (plReadyState == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    *plReadyState = mBrowserReadyState;

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Offline(VARIANT_BOOL __RPC_FAR *pbOffline)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Offline);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pbOffline == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_Offline
    *pbOffline = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Offline(VARIANT_BOOL bOffline)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Offline);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement get_Offline
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Silent(VARIANT_BOOL __RPC_FAR *pbSilent)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Silent);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pbSilent == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //Only really applies to the IE app, not a control
    *pbSilent = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Silent(VARIANT_BOOL bSilent)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Silent);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //Only really applies to the IE app, not a control
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsBrowser(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_RegisterAsBrowser);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pbRegister == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO: Implement get_RegisterAsBrowser
    *pbRegister = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsBrowser(VARIANT_BOOL bRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_RegisterAsBrowser);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO: Implement put_RegisterAsBrowser
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_RegisterAsDropTarget);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pbRegister == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    *pbRegister = mHaveDropTargetFlag ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}



static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    ::RevokeDragDrop(hwnd);
    return TRUE;
}
 

HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_RegisterAsDropTarget(VARIANT_BOOL bRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_RegisterAsDropTarget);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    // Register the window as a drop target
    if (bRegister == VARIANT_TRUE)
    {
        if (!mHaveDropTargetFlag)
        {
            CDropTargetInstance *pDropTarget = NULL;
            CDropTargetInstance::CreateInstance(&pDropTarget);
            if (pDropTarget)
            {
                pDropTarget->AddRef();
                pDropTarget->SetOwner(this);


                // Ask the site if it wants to replace this drop target for another one
                CIPtr(IDropTarget) spDropTarget;
                CIPtr(IDocHostUIHandler) spDocHostUIHandler = m_spClientSite;
                if (spDocHostUIHandler)
                {
                    //if (spDocHostUIHandler->GetDropTarget(pDropTarget, &spDropTarget) != S_OK)
                    if (spDocHostUIHandler->GetDropTarget(pDropTarget, &spDropTarget) == S_OK)
                    {
                        mHaveDropTargetFlag = TRUE;
                        ::RegisterDragDrop(m_hWnd, spDropTarget);
                        //spDropTarget = pDropTarget;
                    }
                }
                else
                //if (spDropTarget)
                {
                    mHaveDropTargetFlag = TRUE;
                    ::RegisterDragDrop(m_hWnd, pDropTarget);
                    //::RegisterDragDrop(m_hWnd, spDropTarget);
                }
                pDropTarget->Release();
            }
            // Now revoke any child window drop targets and pray they aren't
            // reset by the layout engine.
            ::EnumChildWindows(m_hWnd, EnumChildProc, (LPARAM) this);
        }
    }
    else
    {
        if (mHaveDropTargetFlag)
        {
            mHaveDropTargetFlag = FALSE;
            ::RevokeDragDrop(m_hWnd);
        }
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_TheaterMode(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_TheaterMode);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (pbRegister == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TheaterMode doesn't apply to this control.
    *pbRegister = VARIANT_FALSE;

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_TheaterMode(VARIANT_BOOL bRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_TheaterMode);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TheaterMode doesn't apply to this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_AddressBar(VARIANT_BOOL __RPC_FAR *Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_AddressBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (Value == NULL)
    {
        RETURN_E_INVALIDARG();
    }

    //There is no AddressBar in this control.
    *Value = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_AddressBar(VARIANT_BOOL Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_AddressBar);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //There is no AddressBar in this control.
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Resizable(VARIANT_BOOL __RPC_FAR *Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Resizable);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    if (Value == NULL)
    {
        NG_ASSERT(0);
        RETURN_E_INVALIDARG();
    }

    //TODO:  Not sure if this should actually be implemented or not.
    *Value = VARIANT_FALSE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CMozillaBrowser::put_Resizable(VARIANT_BOOL Value)
{
    NG_TRACE_METHOD(CMozillaBrowser::put_Resizable);

    if (!IsValid())
    {
        NG_ASSERT(0);
        RETURN_E_UNEXPECTED();
    }

    //TODO:  Not sure if this should actually be implemented or not.
    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Ole Command Handlers


HRESULT _stdcall CMozillaBrowser::PrintHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    BOOL promptUser = (nCmdexecopt & OLECMDEXECOPT_DONTPROMPTUSER) ? FALSE : TRUE;
    pThis->PrintDocument(promptUser);
    return S_OK;
}


HRESULT _stdcall CMozillaBrowser::EditModeHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    BOOL bEditorMode = (nCmdID == IDM_EDITMODE) ? TRUE : FALSE;
    pThis->SetEditorMode(bEditorMode);
    return S_OK;
}


HRESULT _stdcall CMozillaBrowser::EditCommandHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    pThis->OnEditorCommand(nCmdID);
    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// PrintListener implementation


NS_IMPL_ISUPPORTS1(PrintListener, nsIPrintListener)

PrintListener::PrintListener() : mComplete(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    /* member initializers and constructor code */
}

PrintListener::~PrintListener()
{
    /* destructor code */
}

void PrintListener::WaitForComplete()
{
    MSG msg;
    HANDLE hFakeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

    while (!mComplete)
    {
        // Process pending messages
        while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!::GetMessage(&msg, NULL, 0, 0))
            {
                ::CloseHandle(hFakeEvent);
                return;
            }

            PRBool wasHandled = PR_FALSE;
            ::NS_HandleEmbeddingEvent(msg, wasHandled);
            if (wasHandled)
                continue;

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (mComplete)
            break;
        
        // Do idle stuff
        ::NS_DoIdleEmbeddingStuff();
        ::MsgWaitForMultipleObjects(1, &hFakeEvent, FALSE, 500, QS_ALLEVENTS);
    }

    ::CloseHandle(hFakeEvent);
}

/* void OnStartPrinting (); */
NS_IMETHODIMP PrintListener::OnStartPrinting()
{
    return NS_OK;
}

/* void OnProgressPrinting (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP PrintListener::OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax)
{
    return NS_OK;
}

/* void OnEndPrinting (in PRUint32 aStatus); */
NS_IMETHODIMP PrintListener::OnEndPrinting(PRUint32 aStatus)
{
    mComplete = PR_TRUE;
    return NS_OK;
}
