/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include <string.h>
#include <string>
#include <objidl.h>
//#include <comdef.h>
#include <shlobj.h>

// commdlg.h is needed to build with WIN32_LEAN_AND_MEAN
#include <commdlg.h>

#include "MozillaControl.h"
#include "MozillaBrowser.h"
#include "IEHtmlDocument.h"
#include "PropertyDlg.h"
#include "PageSetupDlg.h"
#include "PromptService.h"
#include "HelperAppDlg.h"
#include "WindowCreator.h"

#include "nsNetUtil.h"
#include "nsCWebBrowser.h"
#include "nsIAtom.h"
#include "nsILocalFile.h"
#include "nsIWebBrowserPersist.h"
#include "nsIClipboardCommands.h"
#include "nsIProfile.h"
#include "nsIPrintOptions.h"
#include "nsIWebBrowserPrint.h"
#include "nsIWidget.h"
#include "nsIWebBrowserFocus.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIDOMWindow.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMNSDocument.h"

#include "nsEmbedAPI.h"

#define HACK_NON_REENTRANCY
#ifdef HACK_NON_REENTRANCY
static HANDLE s_hHackedNonReentrancy = NULL;
#endif

#define NS_PROMPTSERVICE_CID \
  {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}

#define NS_HELPERAPPLAUNCHERDIALOG_CID \
    {0xf68578eb, 0x6ec2, 0x4169, {0xae, 0x19, 0x8c, 0x62, 0x43, 0xf0, 0xab, 0xe1}}

static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);
static NS_DEFINE_CID(kHelperAppLauncherDialogCID, NS_HELPERAPPLAUNCHERDIALOG_CID);

class PrintListener : public nsIWebProgressListener
{
    PRBool mComplete;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER

    PrintListener();
    virtual ~PrintListener();

    void WaitForComplete();
};

class SimpleDirectoryProvider :
    public nsIDirectoryServiceProvider
{
public:
    SimpleDirectoryProvider();
    BOOL IsValid() const;


    NS_DECL_ISUPPORTS
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER

protected:
    virtual ~SimpleDirectoryProvider();

    nsCOMPtr<nsILocalFile> mApplicationRegistryDir;
    nsCOMPtr<nsILocalFile> mApplicationRegistryFile;
    nsCOMPtr<nsILocalFile> mUserProfileDir;
};

// Default page in design mode. The data protocol may or may not be supported
// so the scheme is checked before the page is loaded.

static const char kDesignModeScheme[] = "data";
static const OLECHAR kDesignModeURL[] =
    L"data:text/html,<html><body bgcolor=\"#00FF00\"><p>Mozilla Control</p></body></html>";

// Registry keys and values

static const TCHAR kBrowserHelperObjectRegKey[] =
    _T("Software\\Mozilla\\ActiveX Control\\Browser Helper Objects");


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
    mValidBrowserFlag = FALSE;

    // Create the container that handles some things for us
    mWebBrowserContainer = NULL;

    // Control starts off in non-edit mode
    mEditModeFlag = FALSE;

    // Control starts off without being a drop target
    mHaveDropTargetFlag = FALSE;

     // the IHTMLDocument, lazy allocation.
     mIERootDocument = NULL;

    // Browser helpers
    mBrowserHelperList = NULL;
    mBrowserHelperListCount = 0;

    // Name of the default profile to use
    mProfileName.AssignLiteral("MozillaControl");

    // Initialise the web browser
    Initialize();
}


//
// Destructor
//
CMozillaBrowser::~CMozillaBrowser()
{
    NG_TRACE_METHOD(CMozillaBrowser::~CMozillaBrowser);
    
    // Close the web browser
    Terminate();
}

// See bug 127982:
//
// Microsoft's InlineIsEqualGUID global function is multiply defined
// in ATL and/or SDKs with varying namespace requirements. To save the control
// from future grief, this method is used instead. 
static inline BOOL _IsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
      ((PLONG) &rguid1)[0] == ((PLONG) &rguid2)[0] &&
      ((PLONG) &rguid1)[1] == ((PLONG) &rguid2)[1] &&
      ((PLONG) &rguid1)[2] == ((PLONG) &rguid2)[2] &&
      ((PLONG) &rguid1)[3] == ((PLONG) &rguid2)[3]);
}

STDMETHODIMP CMozillaBrowser::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_IWebBrowser,
        &IID_IWebBrowser2,
        &IID_IWebBrowserApp
    };
    for (int i = 0; i < (sizeof(arr) / sizeof(arr[0])); i++)
    {
        if (_IsEqualGUID(*arr[i], riid))
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
void CMozillaBrowser::ShowURIPropertyDlg(const nsAString &aURI, const nsAString &aContentType)
{
    CPropertyDlg dlg;
    CPPageDlg linkDlg;
    dlg.AddPage(&linkDlg);

    if (!aURI.IsEmpty())
    {
        linkDlg.mType = aContentType;
        linkDlg.mURL = aURI;
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
// Tells the container to change focus to the next control in the dialog.
//
void CMozillaBrowser::NextDlgControl()
{
    HWND hwndParent = GetParent();
    if (::IsWindow(hwndParent))
    {
      ::PostMessage(hwndParent, WM_NEXTDLGCTL, 0, 0);
    }
}


//
// Tells the container to change focus to the previous control in the dialog.
//
void CMozillaBrowser::PrevDlgControl()
{
    HWND hwndParent = GetParent();
    if (::IsWindow(hwndParent))
    {
      ::PostMessage(hwndParent, WM_NEXTDLGCTL, 1, 0);
    }
}


///////////////////////////////////////////////////////////////////////////////
// Message handlers


// Handle WM_CREATE windows message
LRESULT CMozillaBrowser::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnCreate);

    // Create the web browser
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
            // Load a page in design mode if the specified page is supported
            nsCOMPtr<nsIIOService> ios = do_GetIOService();
            if (ios)
            {
                // Ensure design page can be loaded by checking for a
                // registered protocol handler that supports the scheme
                nsCOMPtr<nsIProtocolHandler> ph;
                nsCAutoString phScheme;
                ios->GetProtocolHandler(kDesignModeScheme, getter_AddRefs(ph));
                if (ph &&
                    NS_SUCCEEDED(ph->GetScheme(phScheme)) &&
                    phScheme.EqualsIgnoreCase(kDesignModeScheme))
                {
                    Navigate(const_cast<BSTR>(kDesignModeURL), NULL, NULL, NULL, NULL);
                }
            }
        }
    }

    // Clip the child windows out of paint operations
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_CLIPCHILDREN | WS_TABSTOP);

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

    // Pass resize information down to the browser...
    if (mWebBrowserAsWin)
    {
        mWebBrowserAsWin->SetPosition(rc.left, rc.top);
        mWebBrowserAsWin->SetSize(rc.right - rc.left, rc.bottom - rc.top, PR_TRUE);
    }
    return 0;
}

// Handle WM_SETFOCUS
LRESULT CMozillaBrowser::OnSetFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    ATLTRACE(_T("CMozillaBrowser::OnSetFocus()\n"));
    nsCOMPtr<nsIWebBrowserFocus> browserAsFocus = do_QueryInterface(mWebBrowser);
    if (browserAsFocus)
    {
        browserAsFocus->Activate();
    }
    CComQIPtr<IOleControlSite> controlSite = m_spClientSite;
    if (controlSite)
    {
        controlSite->OnFocus(TRUE);
    }
    return 0;
}

// Handle WM_KILLFOCUS
LRESULT CMozillaBrowser::OnKillFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    ATLTRACE(_T("CMozillaBrowser::OnKillFocus()\n"));
    nsCOMPtr<nsIWebBrowserFocus> browserAsFocus = do_QueryInterface(mWebBrowser);
    if (browserAsFocus)
    {
        browserAsFocus->Deactivate();
    }
    CComQIPtr<IOleControlSite> controlSite = m_spClientSite;
    if (controlSite)
    {
        controlSite->OnFocus(FALSE);
    }
    return 0;
}

// Handle WM_MOUSEACTIVATE messages
LRESULT CMozillaBrowser::OnMouseActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return MA_ACTIVATE;
}

// Handle WM_GETDLGCODE to receive keyboard presses
LRESULT CMozillaBrowser::OnGetDlgCode(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    return DLGC_WANTALLKEYS; 
}

// Handle WM_PAINT windows message (and IViewObject::Draw) 
HRESULT CMozillaBrowser::OnDraw(ATL_DRAWINFO& di)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnDraw);

    if (!BrowserIsValid())
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


    nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mWebBrowser));
    nsCOMPtr<nsIPrintSettings> printSettings;
    if(!print ||
        NS_FAILED(print->GetGlobalPrintSettings(getter_AddRefs(printSettings))))
    {
        return 0;
    }

    // show the page setup dialog
    CPageSetupDlg dlg(printSettings);
    dlg.DoModal();

    return 0;
}

// Handle ID_PRINT command
LRESULT CMozillaBrowser::OnPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnPrint);
    if (!BrowserIsValid())
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
    char szTmp[_MAX_FNAME] = "untitled";
    BSTR pageName = NULL;
    get_LocationName(&pageName); // Page title
    if (pageName)
    {
        USES_CONVERSION;
        strncpy(szTmp, OLE2A(pageName), sizeof(szTmp) - 1);
        SysFreeString(pageName);
        szTmp[sizeof(szTmp) - 1] = '\0';
    }
    
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
        NS_NewNativeLocalFile(nsDependentCString(T2A(szFile)), TRUE, getter_AddRefs(file));

        nsCOMPtr<nsILocalFile> dataPath;
        NS_NewNativeLocalFile(nsDependentCString(szDataPath), TRUE, getter_AddRefs(dataPath));

        persist->SaveDocument(nsnull, file, dataPath, nsnull, 0, 0);
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

    if (!mWebBrowser)
    {
        // No webbrowser to view!
        NG_ASSERT(0);
        return 0;
    }

    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
    if (!webNav)
    {
        // No webnav!
        NG_ASSERT(0);
        return 0;
    }

    nsCOMPtr<nsIURI> uri;
    webNav->GetCurrentURI(getter_AddRefs(uri));
    if (!uri)
    {
        // No URI to view!
        NG_ASSERT(0);
        return 0;
    }

    // Get the current URI
    nsCAutoString aURI;
    uri->GetSpec(aURI);

    nsAutoString strURI(NS_LITERAL_STRING("view-source:"));
    AppendUTF8toUTF16(aURI, strURI);

    // Ask the client to create a window to view the source in
    CIPtr(IDispatch) spDispNew;
    VARIANT_BOOL bCancel = VARIANT_FALSE;
    Fire_NewWindow2(&spDispNew, &bCancel);

    // Load the view-source into a new url
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
    nsAutoString contentType;
    nsCOMPtr<nsIDOMNSDocument> doc = do_QueryInterface(ownerDoc);
    if (doc)
    {
        doc->GetContentType(contentType);
    }

    ShowURIPropertyDlg(uri, contentType);

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

    if (!uri.IsEmpty())
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

    if (!uri.IsEmpty())
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

    if (!uri.IsEmpty() && OpenClipboard())
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
    nsAutoString type;

    nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement = do_QueryInterface(mContextNode);
    if (anchorElement)
    {
        anchorElement->GetHref(uri);
        anchorElement->GetType(type); // How many anchors implement this I wonder
    }

    ShowURIPropertyDlg(uri, type);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////

// Initialises the web browser engine
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

    // Create a simple directory provider. If this fails because the directories are
    // invalid or whatever then the control will fallback on the default directory
    // provider.

    nsCOMPtr<nsIDirectoryServiceProvider> directoryProvider;
    SimpleDirectoryProvider *pDirectoryProvider = new SimpleDirectoryProvider;
    if (pDirectoryProvider->IsValid())
        directoryProvider = do_QueryInterface(pDirectoryProvider);

    // Create an object to represent the path
    nsresult rv;
    nsCOMPtr<nsILocalFile> binDir;
    USES_CONVERSION;
    NS_NewNativeLocalFile(nsDependentCString(T2A(szBinDirPath)), TRUE, getter_AddRefs(binDir));
    rv = NS_InitEmbedding(binDir, directoryProvider);

    // Load preferences service
    mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
        NG_ASSERT(0);
        NG_TRACE_ALWAYS(_T("Could not create preference object rv=%08x\n"), (int) rv);
        SetStartupErrorMessage(IDS_CANNOTCREATEPREFS);
        return E_FAIL;
    }

    // Stuff in here only needs to be done once
    static BOOL bRegisterComponents = FALSE;
    if (!bRegisterComponents)
    {
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

        // Helper app launcher dialog
        nsCOMPtr<nsIFactory> helperAppDlgFactory;
        rv = NS_NewHelperAppLauncherDlgFactory(getter_AddRefs(helperAppDlgFactory));
        rv = nsComponentManager::RegisterFactory(kHelperAppLauncherDialogCID,
            "Helper App Launcher Dialog",
            "@mozilla.org/helperapplauncherdialog;1",
            helperAppDlgFactory,
            PR_TRUE); // replace existing

        // create our local object
        CWindowCreator *creator = new CWindowCreator();
        nsCOMPtr<nsIWindowCreator> windowCreator;
        windowCreator = NS_STATIC_CAST(nsIWindowCreator *, creator);

        // Attach it via the watcher service
        nsCOMPtr<nsIWindowWatcher> watcher =
            do_GetService(NS_WINDOWWATCHER_CONTRACTID);
        if (watcher)
            watcher->SetWindowCreator(windowCreator);
    }

    // Set the profile which the control will use
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
        return E_FAIL;
    }

    // Make a new default profile
    PRBool profileExists = PR_FALSE;
    rv = profileService->ProfileExists(mProfileName.get(), &profileExists);
    if (NS_FAILED(rv))
    {
        return E_FAIL;
    }
    else if (!profileExists)
    {
        rv = profileService->CreateNewProfile(mProfileName.get(), nsnull, nsnull, PR_FALSE);
        if (NS_FAILED(rv))
        {
            return E_FAIL;
        }
    }

    rv = profileService->SetCurrentProfile(mProfileName.get());
    if (NS_FAILED(rv))
    {
        return E_FAIL;
    }

#ifdef HACK_NON_REENTRANCY
    }
#endif

    return S_OK;
}

// Terminates the web browser engine
HRESULT CMozillaBrowser::Terminate()
{
#ifdef HACK_NON_REENTRANCY
    if (0)
    {
#endif

    mPrefBranch = nsnull;
    NS_TermEmbedding();

#ifdef HACK_NON_REENTRANCY
    }
#endif

    return S_OK;
}


// Create and initialise the web browser
HRESULT CMozillaBrowser::CreateBrowser() 
{    
    NG_TRACE_METHOD(CMozillaBrowser::CreateBrowser);
    
    if (mWebBrowser != nsnull)
    {
        NG_ASSERT(0);
        NG_TRACE_ALWAYS(_T("CreateBrowser() called more than once!"));
        return SetErrorInfo(E_UNEXPECTED);
    }

    RECT rcLocation;
    GetClientRect(&rcLocation);
    if (IsRectEmpty(&rcLocation))
    {
        rcLocation.bottom++;
        rcLocation.top++;
    }

    nsresult rv;

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

    // Allow plugins?
    const PRBool kAllowPlugins = PR_TRUE;
    webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, kAllowPlugins);

    // Host chrome or content?
    const PRBool kHostChrome = PR_FALSE; // Hardcoded for now
    webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER, PR_FALSE);

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
        do_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, mWebBrowserContainer)));
    mWebBrowser->AddWebBrowserListener(listener, NS_GET_IID(nsIWebProgressListener));

    // Visible
    mWebBrowserAsWin->SetVisibility(PR_TRUE);

    // Activated
    nsCOMPtr<nsIWebBrowserFocus> browserAsFocus = do_QueryInterface(mWebBrowser);
    browserAsFocus->Activate();

    // Get an editor session
    mEditingSession = do_GetInterface(mWebBrowser);
    mCommandManager = do_GetInterface(mWebBrowser);

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

    mEditingSession = nsnull;
    mCommandManager = nsnull;
    mWebBrowser = nsnull;

    return S_OK;
}


// Turns the editor mode on or off
HRESULT CMozillaBrowser::SetEditorMode(BOOL bEnabled)
{
    NG_TRACE_METHOD(CMozillaBrowser::SetEditorMode);

    if (!mEditingSession || !mCommandManager)
        return E_FAIL;
  
    nsCOMPtr<nsIDOMWindow> domWindow;
    nsresult rv = GetDOMWindow(getter_AddRefs(domWindow));
    if (NS_FAILED(rv))
        return E_FAIL;

    rv = mEditingSession->MakeWindowEditable(domWindow, "html", PR_FALSE);
 
    return S_OK;
}


HRESULT CMozillaBrowser::OnEditorCommand(DWORD nCmdID)
{
    NG_TRACE_METHOD(CMozillaBrowser::OnEditorCommand);

    nsCOMPtr<nsIDOMWindow> domWindow;
    GetDOMWindow(getter_AddRefs(domWindow));

    const char *styleCommand = nsnull;
    nsICommandParams *commandParams = nsnull;

    switch (nCmdID)
    {
    case IDM_BOLD:
        styleCommand = "cmd_bold";
        break;
    case IDM_ITALIC:
        styleCommand = "cmd_italic";
        break;
    case IDM_UNDERLINE:
        styleCommand = "cmd_underline";
        break;
    
    // TODO add the rest!
    
    default:
        // DO NOTHING
        break;
    }

    return mCommandManager ?
        mCommandManager->DoCommand(styleCommand, commandParams, domWindow) :
        NS_ERROR_FAILURE;
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

    if (!BrowserIsValid())
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
    if (cKey.Open(HKEY_LOCAL_MACHINE, kBrowserHelperObjectRegKey, KEY_ENUMERATE_SUB_KEYS) != ERROR_SUCCESS)
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

        NG_TRACE(_T("Reading helper object entry \"%s\"\n"), szCLSID);

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
        else
        {
            // Set the object to point at the browser
            cpObjectWithSite->SetSite((IWebBrowser2 *) this);

            // Store in the list
            CComUnkPtr cpUnk = cpObjectWithSite;
            mBrowserHelperList[mBrowserHelperListCount++] = cpUnk;
        }
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
    // Print the contents
    nsCOMPtr<nsIWebBrowserPrint> browserAsPrint = do_GetInterface(mWebBrowser);
    NS_ASSERTION(browserAsPrint, "No nsIWebBrowserPrint!");

    PRBool oldPrintSilent = PR_FALSE;
    nsCOMPtr<nsIPrintSettings> printSettings;
    browserAsPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
    if (printSettings) 
    {
        printSettings->GetPrintSilent(&oldPrintSilent);
        printSettings->SetPrintSilent(promptUser ? PR_FALSE : PR_TRUE);
    }

    // Disable print progress dialog (XUL)
    PRBool oldShowPrintProgress = FALSE;
    const char *kShowPrintProgressPref = "print.show_print_progress";
    mPrefBranch->GetBoolPref(kShowPrintProgressPref, &oldShowPrintProgress);
    mPrefBranch->SetBoolPref(kShowPrintProgressPref, PR_FALSE);

    // Print
    PrintListener *listener = new PrintListener;
    nsCOMPtr<nsIWebProgressListener> printListener = do_QueryInterface(listener);
    nsresult rv = browserAsPrint->Print(printSettings, printListener);
    if (NS_SUCCEEDED(rv))
    {
        listener->WaitForComplete();
    }

    // Cleanup
    if (printSettings)
    {
        printSettings->SetPrintSilent(oldPrintSilent);
    }
    mPrefBranch->SetBoolPref(kShowPrintProgressPref, oldShowPrintProgress);

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
        return SetErrorInfo(E_INVALIDARG);
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
// IWebBrowserImpl

nsresult CMozillaBrowser::GetWebNavigation(nsIWebNavigation **aWebNav)
{
    NS_ENSURE_ARG_POINTER(aWebNav);
    if (!mWebBrowser) return NS_ERROR_FAILURE;
    return mWebBrowser->QueryInterface(NS_GET_IID(nsIWebNavigation), (void **) aWebNav);
}

nsresult CMozillaBrowser::GetDOMWindow(nsIDOMWindow **aDOMWindow)
{
    NS_ENSURE_ARG_POINTER(aDOMWindow);
    if (!mWebBrowser) return NS_ERROR_FAILURE;
    return mWebBrowser->GetContentDOMWindow(aDOMWindow);
}

nsresult CMozillaBrowser::GetPrefs(nsIPrefBranch **aPrefBranch)
{
    if (mPrefBranch)
        *aPrefBranch = mPrefBranch;
    NS_IF_ADDREF(*aPrefBranch);
    return (*aPrefBranch) ? NS_OK : NS_ERROR_FAILURE;
}

PRBool CMozillaBrowser::BrowserIsValid()
{
    NG_TRACE_METHOD(CMozillaBrowser::BrowserIsValid);
    return mValidBrowserFlag ? PR_TRUE : PR_FALSE;
}

// Overrides of IWebBrowser / IWebBrowserApp / IWebBrowser2 methods

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Parent);
    ENSURE_BROWSER_IS_VALID();

    if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
    {
        NG_ASSERT(0);
        return SetErrorInfo(E_INVALIDARG);
    }

    // Attempt to get the parent object of this control
    HRESULT hr = E_FAIL;
    if (m_spClientSite)
    {
        hr = m_spClientSite->QueryInterface(IID_IDispatch, (void **) ppDisp);
    }

    return (SUCCEEDED(hr)) ? S_OK : E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_Document);
    ENSURE_BROWSER_IS_VALID();

    if (!NgIsValidAddress(ppDisp, sizeof(IDispatch *)))
    {
        NG_ASSERT(0);
        return SetErrorInfo(E_UNEXPECTED);
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
            return SetErrorInfo(E_FAIL, L"get_Document: not HTML");
        }

         CIEHtmlDocumentInstance::CreateInstance(&mIERootDocument);
          if (mIERootDocument == NULL)
         {
            return SetErrorInfo(E_OUTOFMEMORY, L"get_Document: can't create IERootDocument");
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

HRESULT STDMETHODCALLTYPE CMozillaBrowser::get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister)
{
    NG_TRACE_METHOD(CMozillaBrowser::get_RegisterAsDropTarget);
    ENSURE_BROWSER_IS_VALID();

    if (pbRegister == NULL)
    {
        NG_ASSERT(0);
        return SetErrorInfo(E_INVALIDARG);
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
    ENSURE_BROWSER_IS_VALID();

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
// SimpleDirectoryProvider implementation

SimpleDirectoryProvider::SimpleDirectoryProvider()
{
    nsCOMPtr<nsILocalFile> appDataDir;

    // Attempt to fill appDataDir with a meaningful value. Any error in the process
    // will cause the constructor to return and IsValid() to return FALSE,

    CComPtr<IMalloc> shellMalloc;
    SHGetMalloc(&shellMalloc);
    if (shellMalloc)
    {
        LPITEMIDLIST pitemidList = NULL;
        SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pitemidList);
        if (pitemidList)
        {
            TCHAR szBuffer[MAX_PATH + 1];
            if (SUCCEEDED(SHGetPathFromIDList(pitemidList, szBuffer)))
            {
                szBuffer[MAX_PATH] = TCHAR('\0');
                USES_CONVERSION;
                NS_NewNativeLocalFile(nsDependentCString(T2A(szBuffer)), TRUE, getter_AddRefs(appDataDir));
            }
            shellMalloc->Free(pitemidList);
        }
    }
    if (!appDataDir)
    {
        return;
    }

    // Mozilla control paths are
    // App data     - {Application Data}/MozillaControl
    // App registry - {Application Data}/MozillaControl/registry.dat
    // Profiles     - {Application Data}/MozillaControl/profiles

    nsresult rv;

    // Create the root directory
    PRBool exists;
    rv = appDataDir->Exists(&exists);
    if (NS_FAILED(rv) || !exists) return;

    // MozillaControl application data
    rv = appDataDir->AppendRelativePath(NS_LITERAL_STRING("MozillaControl"));
    if (NS_FAILED(rv)) return;
    rv = appDataDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = appDataDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return;

    // Registry.dat file
    nsCOMPtr<nsIFile> appDataRegAsFile;
    rv = appDataDir->Clone(getter_AddRefs(appDataRegAsFile));
    if (NS_FAILED(rv)) return;
    nsCOMPtr<nsILocalFile> appDataRegistry = do_QueryInterface(appDataRegAsFile, &rv);
    if (NS_FAILED(rv)) return;
    appDataRegistry->AppendRelativePath(NS_LITERAL_STRING("registry.dat"));

    // Profiles directory
    nsCOMPtr<nsIFile> profileDirAsFile;
    rv = appDataDir->Clone(getter_AddRefs(profileDirAsFile));
    if (NS_FAILED(rv)) return;
    nsCOMPtr<nsILocalFile> profileDir = do_QueryInterface(profileDirAsFile, &rv);
    if (NS_FAILED(rv)) return;
    profileDir->AppendRelativePath(NS_LITERAL_STRING("profiles"));
    rv = profileDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return;

    // Store the member values
    mApplicationRegistryDir = appDataDir;
    mApplicationRegistryFile = appDataRegistry;
    mUserProfileDir = profileDir;
}

SimpleDirectoryProvider::~SimpleDirectoryProvider()
{
}

BOOL
SimpleDirectoryProvider::IsValid() const
{
    return (mApplicationRegistryDir && mApplicationRegistryFile && mUserProfileDir) ?
        TRUE : FALSE;
}

NS_IMPL_ISUPPORTS1(SimpleDirectoryProvider, nsIDirectoryServiceProvider)

///////////////////////////////////////////////////////////////////////////////
// nsIDirectoryServiceProvider

/* nsIFile getFile (in string prop, out PRBool persistent); */
NS_IMETHODIMP SimpleDirectoryProvider::GetFile(const char *prop, PRBool *persistent, nsIFile **_retval)
{
    NS_ENSURE_ARG_POINTER(prop);
    NS_ENSURE_ARG_POINTER(persistent);
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nsnull;
    *persistent = PR_TRUE;

    // Only need to support NS_APP_APPLICATION_REGISTRY_DIR, NS_APP_APPLICATION_REGISTRY_FILE, and
    // NS_APP_USER_PROFILES_ROOT_DIR. Unsupported keys fallback to the default service provider
    
    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv = NS_ERROR_FAILURE;

    if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
    {
        localFile = mApplicationRegistryDir;
    }
    else if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
    {
        localFile = mApplicationRegistryFile;
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
    {
        localFile = mUserProfileDir;
    }
    
    if (localFile)
        return CallQueryInterface(localFile, _retval);
        
    return rv;
}


///////////////////////////////////////////////////////////////////////////////
// PrintListener implementation


NS_IMPL_ISUPPORTS1(PrintListener, nsIWebProgressListener)

PrintListener::PrintListener() : mComplete(PR_FALSE)
{
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
        ::MsgWaitForMultipleObjects(1, &hFakeEvent, FALSE, 500, QS_ALLEVENTS);
    }

    ::CloseHandle(hFakeEvent);
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP PrintListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    if (aStateFlags & nsIWebProgressListener::STATE_STOP &&
        aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
        mComplete = PR_TRUE;
    }
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP PrintListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP PrintListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP PrintListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP PrintListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}
