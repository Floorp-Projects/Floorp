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
 *   Doug Turner <dougt@netscape.com>
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// Win32 header files
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

// Mozilla Frozen APIs
#include "nsXULAppAPI.h"

XRE_InitEmbedding2Type XRE_InitEmbedding2;
XRE_TermEmbeddingType XRE_TermEmbedding;

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsProfileDirServiceProvider.h"
#include "nsStringAPI.h"
#include "nsXPCOMGlue.h"

#include "nsIClipboardCommands.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIURI.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWindowWatcher.h"

// NON-FROZEN APIs!
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"

// Local header files
#include "winEmbed.h"
#include "WebBrowserChrome.h"
#include "WindowCreator.h"
#include "resource.h"

#define MAX_LOADSTRING 100

const TCHAR *szWindowClass = _T("WINEMBED");

// Foward declarations of functions included in this code module:
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static LRESULT CALLBACK BrowserWndProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static nsresult InitializeWindowCreator();
static nsresult OpenWebPage(const char * url);
static nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome);

// Profile chooser stuff
static nsresult StartupProfile();

// Global variables
static UINT gDialogCount = 0;
static HINSTANCE ghInstanceApp = NULL;
static char gFirstURL[1024];

// like strpbrk but finds the *last* char, not the first
static char*
ns_strrpbrk(char *string, const char *strCharSet)
{
    char *found = NULL;
    for (; *string; ++string) {
        for (const char *search = strCharSet; *search; ++search) {
            if (*search == *string) {
                found = string;
                // Since we're looking for the last char, we save "found"
                // until we're at the end of the string.
            }
        }
    }

    return found;
}

// A list of URLs to populate the URL drop down list with
static const TCHAR *gDefaultURLs[] = 
{
    _T("http://www.mozilla.org/"),
    _T("http://www.netscape.com/"),
    _T("http://browsertest.web.aol.com/tests/javascript/javascpt/index.htm"),
    _T("http://127.0.0.1/"),
    _T("http://www.yahoo.com/"),
    _T("http://www.travelocity.com/"),
    _T("http://www.disney.com/"),
    _T("http://www.go.com/"),
    _T("http://www.google.com/"),
    _T("http://www.ebay.com/"),
    _T("http://www.shockwave.com/"),
    _T("http://www.slashdot.org/"),
    _T("http://www.quicken.com/"),
    _T("http://www.hotmail.com/"),
    _T("http://www.cnn.com/"),
    _T("http://www.javasoft.com/")
};

int main(int argc, char *argv[])
{
    nsresult rv;

    printf("You are embedded, man!\n\n");
    printf("******************************************************************\n");
    printf("*                                                                *\n");
    printf("*  IMPORTANT NOTE:                                               *\n");
    printf("*                                                                *\n");
    printf("*  WinEmbed is not supported!!! Do not raise bugs on it unless   *\n");
    printf("*  it is badly broken (e.g. crash on start/exit, build errors)   *\n");
    printf("*  or you have the patch to make it better! MFCEmbed is now our  *\n");
    printf("*  embedding test application on Win32 and all testing should    *\n");
    printf("*  be done on that.                                              *\n");
    printf("*                                                                *\n");
    printf("******************************************************************\n");
    printf("\n\n");
    
    // Sophisticated command-line parsing in action
    char *szFirstURL = "http://www.mozilla.org/projects/embedding/";
	int argn;
    for (argn = 1; argn < argc; argn++)
    {
		szFirstURL = argv[argn];
    }
    strncpy(gFirstURL, szFirstURL, sizeof(gFirstURL) - 1);

    ghInstanceApp = GetModuleHandle(NULL);

    // Initialize global strings
    TCHAR szTitle[MAX_LOADSTRING];
    LoadString(ghInstanceApp, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    MyRegisterClass(ghInstanceApp);

    char path[_MAX_PATH];
    GetModuleFileName(ghInstanceApp, path, sizeof(path));
    char* lastslash = ns_strrpbrk(path, "/\\");
    if (!lastslash)
        return 7;

    strcpy(lastslash, "\\xulrunner\\xpcom.dll");

    rv = XPCOMGlueStartup(path);
    if (NS_FAILED(rv))
        return 3;

    strcpy(lastslash, "\\xulrunner\\xul.dll");

    HINSTANCE xulModule = LoadLibraryEx(path, NULL, 0);
    if (!xulModule)
        return 4;

    XRE_InitEmbedding2 =
        (XRE_InitEmbedding2Type) GetProcAddress(xulModule, "XRE_InitEmbedding2");
    if (!XRE_InitEmbedding2) {
        fprintf(stderr, "Error: %i\n", GetLastError());
        return 5;
    }

    XRE_TermEmbedding =
        (XRE_TermEmbeddingType) GetProcAddress(xulModule, "XRE_TermEmbedding");
    if (!XRE_TermEmbedding) {
        fprintf(stderr, "Error: %i\n", GetLastError());
        return 5;
    }

    // Scope all the XPCOM stuff
    {
        strcpy(lastslash, "\\xulrunner");

        nsCOMPtr<nsILocalFile> xuldir;
        rv = NS_NewNativeLocalFile(nsCString(path), PR_FALSE,
                                   getter_AddRefs(xuldir));
        if (NS_FAILED(rv))
            return 6;

        *lastslash = '\0';

        nsCOMPtr<nsILocalFile> appdir;
        rv = NS_NewNativeLocalFile(nsCString(path), PR_FALSE,
                                   getter_AddRefs(appdir));
        if (NS_FAILED(rv))
            return 8;

        rv = XRE_InitEmbedding2(xuldir, appdir, nsnull);
        if (NS_FAILED(rv))
            return 9;

        int result = 0;
        if (NS_FAILED(StartupProfile())) {
            result = 8;
        }
        else {
            InitializeWindowCreator();

            // Open the initial browser window
            OpenWebPage(gFirstURL);

            // Main message loop.
            // NOTE: We use a fake event and a timeout in order to process idle stuff for
            //       Mozilla every 1/10th of a second.
            bool runCondition = true;

            rv = AppCallbacks::RunEventLoop(runCondition);
        }
    }
    XRE_TermEmbedding();

    return rv;
}

/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This is how all new windows are opened,
   except any created directly by the embedding app. */
nsresult
InitializeWindowCreator()
{
    // create an nsWindowCreator and give it to the WindowWatcher service
    nsCOMPtr<nsIWindowCreator> creator(new WindowCreator());
    if (!creator)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (!wwatch)
        return NS_ERROR_UNEXPECTED;

    return wwatch->SetWindowCreator(creator);
}

//-----------------------------------------------------------------------------

//
//  FUNCTION: OpenWebPage()
//
//  PURPOSE: Opens a new browser dialog and starts it loading to the
//           specified url.
//
nsresult OpenWebPage(const char *url)
{
    nsresult  rv;

    // Create the chrome object. Note that it leaves this function
    // with an extra reference so that it can released correctly during
    // destruction (via Win32UI::Destroy)

    nsCOMPtr<nsIWebBrowserChrome> chrome;
    rv = AppCallbacks::CreateBrowserWindow(nsIWebBrowserChrome::CHROME_ALL,
           nsnull, getter_AddRefs(chrome));
    if (NS_SUCCEEDED(rv))
    {
        // Start loading a page
        nsCOMPtr<nsIWebBrowser> newBrowser;
        chrome->GetWebBrowser(getter_AddRefs(newBrowser));
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(newBrowser));

        return webNav->LoadURI(NS_ConvertASCIItoUTF16(url).get(),
                               nsIWebNavigation::LOAD_FLAGS_NONE,
                               nsnull,
                               nsnull,
                               nsnull);
    }

    return rv;
}   

//
//  FUNCTION: GetBrowserFromChrome()
//
//  PURPOSE: Returns the HWND for the webbrowser container associated
//           with the specified chrome.
//
HWND GetBrowserFromChrome(nsIWebBrowserChrome *aChrome)
{
    if (!aChrome)
    {
        return NULL;
    }
    nsCOMPtr<nsIEmbeddingSiteWindow> baseWindow = do_QueryInterface(aChrome);
    HWND hwnd = NULL;
    baseWindow->GetSiteWindow((void **) & hwnd);
    return hwnd;
}


//
//  FUNCTION: GetBrowserDlgFromChrome()
//
//  PURPOSE: Returns the HWND for the browser dialog associated with
//           the specified chrome.
//
HWND GetBrowserDlgFromChrome(nsIWebBrowserChrome *aChrome)
{
    return GetParent(GetBrowserFromChrome(aChrome));
}


//
//  FUNCTION: ResizeEmbedding()
//
//  PURPOSE: Resizes the webbrowser window to fit its container.
//
nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome)
{
    if (!chrome)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingSite = do_QueryInterface(chrome);
    HWND hWnd;
    embeddingSite->GetSiteWindow((void **) & hWnd);
    
    if (!hWnd)
        return NS_ERROR_NULL_POINTER;
    
    RECT rect;
    GetClientRect(hWnd, &rect);
    
    // Make sure the browser is visible and sized
    nsCOMPtr<nsIWebBrowser> webBrowser;
    chrome->GetWebBrowser(getter_AddRefs(webBrowser));
    nsCOMPtr<nsIBaseWindow> webBrowserAsWin = do_QueryInterface(webBrowser);
    if (webBrowserAsWin)
    {
        webBrowserAsWin->SetPositionAndSize(rect.left, 
                                   rect.top, 
                                   rect.right - rect.left, 
                                   rect.bottom - rect.top,
                                   PR_TRUE);
        webBrowserAsWin->SetVisibility(PR_TRUE);
    }

    return NS_OK;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEX); 

    wcex.style            = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = (WNDPROC) BrowserWndProc;
    wcex.cbClsExtra        = 0;
    wcex.cbWndExtra        = 0;
    wcex.hInstance        = hInstance;
    wcex.hIcon            = LoadIcon(ghInstanceApp, (LPCTSTR)IDI_WINEMBED);
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground    = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName    = szWindowClass;
    wcex.hIconSm        = LoadIcon(ghInstanceApp, (LPCTSTR)IDI_SMALL);

    return RegisterClassEx(&wcex);
}


//
//  FUNCTION: UpdateUI()
//
//  PURPOSE: Refreshes the buttons and menu items in the browser dialog
//
void UpdateUI(nsIWebBrowserChrome *aChrome)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    aChrome->GetWebBrowser(getter_AddRefs(webBrowser));
    webNavigation = do_QueryInterface(webBrowser);

    bool canGoBack = false;
    bool canGoForward = false;
    if (webNavigation)
    {
        webNavigation->GetCanGoBack(&canGoBack);
        webNavigation->GetCanGoForward(&canGoForward);
    }

    bool canCutSelection = false;
    bool canCopySelection = false;
    bool canPaste = false;

    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
    if (clipCmds)
    {
        clipCmds->CanCutSelection(&canCutSelection);
        clipCmds->CanCopySelection(&canCopySelection);
        clipCmds->CanPaste(&canPaste);
    }

    HMENU hmenu = GetMenu(hwndDlg);
    if (hmenu)
    {
        EnableMenuItem(hmenu, MOZ_GoBack, MF_BYCOMMAND |
            ((canGoBack) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
        EnableMenuItem(hmenu, MOZ_GoForward, MF_BYCOMMAND |
            ((canGoForward) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));

        EnableMenuItem(hmenu, MOZ_Cut, MF_BYCOMMAND |
            ((canCutSelection) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
        EnableMenuItem(hmenu, MOZ_Copy, MF_BYCOMMAND |
            ((canCopySelection) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
        EnableMenuItem(hmenu, MOZ_Paste, MF_BYCOMMAND |
            ((canPaste) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
    }

    HWND button;
    button = GetDlgItem(hwndDlg, IDC_BACK);
    if (button)
      EnableWindow(button, canGoBack);
    button = GetDlgItem(hwndDlg, IDC_FORWARD);
    if (button)
      EnableWindow(button, canGoForward);
}


//
//  FUNCTION: BrowserDlgProc()
//
//  PURPOSE: Browser dialog windows message handler.
//
//  COMMENTS:
//
//    The code for handling buttons and menu actions is here.
//
INT_PTR CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Get the browser and other pointers since they are used a lot below
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    nsIWebBrowserChrome *chrome = nsnull ;
    if (hwndBrowser)
    {
        chrome = (nsIWebBrowserChrome *) GetWindowLongPtr(hwndBrowser, GWLP_USERDATA);
    }
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    if (chrome)
    {
        chrome->GetWebBrowser(getter_AddRefs(webBrowser));
        webNavigation = do_QueryInterface(webBrowser);
    }

    // Test the message
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_INITMENU:
        UpdateUI(chrome);
        return TRUE;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            WebBrowserChromeUI::Destroy(chrome);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        return TRUE;

    case WM_COMMAND:
        if (!webBrowser)
        {
            return TRUE;
        }

        // Test which command was selected
        switch (LOWORD(wParam))
        {
        case IDC_ADDRESS:
            if (HIWORD(wParam) == CBN_EDITCHANGE || HIWORD(wParam) == CBN_SELCHANGE)
            {
                // User has changed the address field so enable the Go button
                EnableWindow(GetDlgItem(hwndDlg, IDC_GO), TRUE);
            }
            break;

        case IDC_GO:
            {
                TCHAR szURL[2048];
                memset(szURL, 0, sizeof(szURL));
                GetDlgItemText(hwndDlg, IDC_ADDRESS, szURL,
                    sizeof(szURL) / sizeof(szURL[0]) - 1);
                webNavigation->LoadURI(
                    NS_ConvertASCIItoUTF16(szURL).get(),
                    nsIWebNavigation::LOAD_FLAGS_NONE,
                    nsnull,
                    nsnull,
                    nsnull);
            }
            break;

        case IDC_STOP:
            webNavigation->Stop(nsIWebNavigation::STOP_ALL);
            UpdateUI(chrome);
            break;

        case IDC_RELOAD:
            webNavigation->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
            break;

        case IDM_EXIT:
            PostMessage(hwndDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;

        // File menu commands

        case MOZ_NewBrowser:
            OpenWebPage(gFirstURL);
            break;

        // Edit menu commands

        case MOZ_Cut:
            {
                nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
                clipCmds->CutSelection();
            }
            break;

        case MOZ_Copy:
            {
                nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
                clipCmds->CopySelection();
            }
            break;

        case MOZ_Paste:
            {
                nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
                clipCmds->Paste();
            }
            break;

        case MOZ_SelectAll:
            {
                nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
                clipCmds->SelectAll();
            }
            break;

        case MOZ_SelectNone:
            {
                nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
                clipCmds->SelectNone();
            }
            break;

        // Go menu commands
        case IDC_BACK:
        case MOZ_GoBack:
            webNavigation->GoBack();
            UpdateUI(chrome);
            break;

        case IDC_FORWARD:
        case MOZ_GoForward:
            webNavigation->GoForward();
            UpdateUI(chrome);
            break;

        // Help menu commands
        case MOZ_About:
            {
                TCHAR szAboutTitle[MAX_LOADSTRING];
                TCHAR szAbout[MAX_LOADSTRING];
                LoadString(ghInstanceApp, IDS_ABOUT_TITLE, szAboutTitle, MAX_LOADSTRING);
                LoadString(ghInstanceApp, IDS_ABOUT, szAbout, MAX_LOADSTRING);
                MessageBox(NULL, szAbout, szAboutTitle, MB_OK);
            }
            break;
        }

        return TRUE;

    case WM_ACTIVATE:
        {
            nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(webBrowser));
            if(focus)
            {
                switch (wParam)
                {
                case WA_ACTIVE:
                    focus->Activate();
                    break;
                case WA_INACTIVE:
                    focus->Deactivate();
                    break;
                default:
                    break;
                }
            }
        }
        break;

    case WM_SIZE:
        {
            UINT newDlgWidth = LOWORD(lParam);
            UINT newDlgHeight = HIWORD(lParam);

            // TODO Reposition the control bar - for the moment it's fixed size

            // Reposition the status area. Status bar
            // gets any space that the fixed size progress bar doesn't use.
            int progressWidth;
            int statusWidth;
            int statusHeight;
            HWND hwndStatus = GetDlgItem(hwndDlg, IDC_STATUS);
            if (hwndStatus) {
              RECT rcStatus;
              GetWindowRect(hwndStatus, &rcStatus);
              statusHeight = rcStatus.bottom - rcStatus.top;
            } else
              statusHeight = 0;

            HWND hwndProgress = GetDlgItem(hwndDlg, IDC_PROGRESS);
            if (hwndProgress) {
              RECT rcProgress;
              GetWindowRect(hwndProgress, &rcProgress);
              progressWidth = rcProgress.right - rcProgress.left;
            } else
              progressWidth = 0;
            statusWidth = newDlgWidth - progressWidth;

            if (hwndStatus)
              SetWindowPos(hwndStatus,
                           HWND_TOP,
                           0, newDlgHeight - statusHeight,
                           statusWidth,
                           statusHeight,
                           SWP_NOZORDER);
            if (hwndProgress)
              SetWindowPos(hwndProgress,
                           HWND_TOP,
                           statusWidth, newDlgHeight - statusHeight,
                           0, 0,
                           SWP_NOSIZE | SWP_NOZORDER);

            // Resize the browser area (assuming the browse is
            // sandwiched between the control bar and status area)
            RECT rcBrowser;
            POINT ptBrowser;
            GetWindowRect(hwndBrowser, &rcBrowser);
            ptBrowser.x = rcBrowser.left;
            ptBrowser.y = rcBrowser.top;
            ScreenToClient(hwndDlg, &ptBrowser);
            int browserHeight = newDlgHeight - ptBrowser.y - statusHeight;
            if (browserHeight < 1)
            {
                browserHeight = 1;
            }
            SetWindowPos(hwndBrowser,
                         HWND_TOP,
                         0, 0,
                         newDlgWidth,
                         newDlgHeight - ptBrowser.y - statusHeight,
                         SWP_NOMOVE | SWP_NOZORDER);
        }
        return TRUE;
    }
    return FALSE;
}


//
//  FUNCTION: BrowserWndProc(HWND, UINT, WRAPAM, LPARAM)
//
//  PURPOSE:  Processes messages for the browser container window.
//
LRESULT CALLBACK BrowserWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    nsIWebBrowserChrome *chrome = (nsIWebBrowserChrome *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (message) 
    {
    case WM_SIZE:
        // Resize the embedded browser
        ResizeEmbedding(chrome);
        return 0;
    case WM_ERASEBKGND:
        // Reduce flicker by not painting the non-visible background
        return 1;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

//
//  FUNCTION: StartupProfile()
//
//  PURPOSE: 
//
nsresult StartupProfile()
{

	nsCOMPtr<nsIFile> appDataDir;
	nsresult rv = NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR, getter_AddRefs(appDataDir));
	if (NS_FAILED(rv))
      return rv;

	appDataDir->AppendNative(nsCString("winembed"));
	nsCOMPtr<nsILocalFile> localAppDataDir(do_QueryInterface(appDataDir));

	nsCOMPtr<nsProfileDirServiceProvider> locProvider;
    NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(locProvider));
    if (!locProvider)
      return NS_ERROR_FAILURE;
    
	rv = locProvider->Register();
    if (NS_FAILED(rv))
      return rv;
    
	return locProvider->SetProfileDir(localAppDataDir);

}


///////////////////////////////////////////////////////////////////////////////
// WebBrowserChromeUI

//
//  FUNCTION: CreateNativeWindow()
//
//  PURPOSE: Creates a new browser dialog.
//
//  COMMENTS:
//
//    This function loads the browser dialog from a resource template
//    and returns the HWND for the webbrowser container dialog item
//    to the caller.
//
HWND WebBrowserChromeUI::CreateNativeWindow(nsIWebBrowserChrome* chrome)
{
  // Load the browser dialog from resource
  HWND hwndDialog;
  PRUint32 chromeFlags;

  chrome->GetChromeFlags(&chromeFlags);
  if ((chromeFlags & nsIWebBrowserChrome::CHROME_ALL) == nsIWebBrowserChrome::CHROME_ALL)
    hwndDialog = CreateDialog(ghInstanceApp,
                              MAKEINTRESOURCE(IDD_BROWSER),
                              NULL,
                              BrowserDlgProc);
  else
    hwndDialog = CreateDialog(ghInstanceApp,
                              MAKEINTRESOURCE(IDD_BROWSER_NC),
                              NULL,
                              BrowserDlgProc);
  if (!hwndDialog)
    return NULL;

  // Stick a menu onto it
  if (chromeFlags & nsIWebBrowserChrome::CHROME_MENUBAR) {
    HMENU hmenuDlg = LoadMenu(ghInstanceApp, MAKEINTRESOURCE(IDC_WINEMBED));
    SetMenu(hwndDialog, hmenuDlg);
  } else
    SetMenu(hwndDialog, 0);

  // Add some interesting URLs to the address drop down
  HWND hwndAddress = GetDlgItem(hwndDialog, IDC_ADDRESS);
  if (hwndAddress) {
    for (int i = 0; i < sizeof(gDefaultURLs) / sizeof(gDefaultURLs[0]); i++)
    {
      SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) gDefaultURLs[i]);
    }
  }

  // Fetch the browser window handle
  HWND hwndBrowser = GetDlgItem(hwndDialog, IDC_BROWSER);
  SetWindowLongPtr(hwndBrowser, GWLP_USERDATA, (LONG_PTR)chrome);  // save the browser LONG_PTR.
  SetWindowLongPtr(hwndBrowser, GWL_STYLE, GetWindowLongPtr(hwndBrowser, GWL_STYLE) | WS_CLIPCHILDREN);

  // Activate the window
  PostMessage(hwndDialog, WM_ACTIVATE, WA_ACTIVE, 0);

  gDialogCount++;

  return hwndBrowser;
}


//
// FUNCTION: Destroy()
//
// PURPOSE: Destroy the window specified by the chrome
//
void WebBrowserChromeUI::Destroy(nsIWebBrowserChrome* chrome)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  nsCOMPtr<nsIWebNavigation> webNavigation;

  chrome->GetWebBrowser(getter_AddRefs(webBrowser));
  webNavigation = do_QueryInterface(webBrowser);
  if (webNavigation)
    webNavigation->Stop(nsIWebNavigation::STOP_ALL);

  chrome->ExitModalEventLoop(NS_OK);

  HWND hwndDlg = GetBrowserDlgFromChrome(chrome);
  if (hwndDlg == NULL)
    return;

  // Explicitly destroy the embedded browser and then the chrome

  // First the browser
  nsCOMPtr<nsIWebBrowser> browser = nsnull;
  chrome->GetWebBrowser(getter_AddRefs(browser));
  nsCOMPtr<nsIBaseWindow> browserAsWin = do_QueryInterface(browser);
  if (browserAsWin)
    browserAsWin->Destroy();

      // Now the chrome
  chrome->SetWebBrowser(nsnull);
  NS_RELEASE(chrome);
}


//
// FUNCTION: Called as the final act of a chrome object during its destructor
//
void WebBrowserChromeUI::Destroyed(nsIWebBrowserChrome* chrome)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(chrome);
    if (hwndDlg == NULL)
    {
        return;
    }

    // Clear the window user data
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    SetWindowLongPtr(hwndBrowser, GWLP_USERDATA, nsnull);
    DestroyWindow(hwndBrowser);
    DestroyWindow(hwndDlg);

    --gDialogCount;
    if (gDialogCount == 0)
    {
        // Quit when there are no more browser objects
        PostQuitMessage(0);
    }
}


//
// FUNCTION: Set the input focus onto the browser window
//
void WebBrowserChromeUI::SetFocus(nsIWebBrowserChrome *chrome)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(chrome);
    if (hwndDlg == NULL)
    {
        return;
    }
    
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    ::SetFocus(hwndBrowser);
}

//
//  FUNCTION: UpdateStatusBarText()
//
//  PURPOSE: Set the status bar text.
//
void WebBrowserChromeUI::UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    nsCString status; 
    if (aStatusText) {
        nsString wStatusText(aStatusText);
        NS_UTF16ToCString(wStatusText, NS_CSTRING_ENCODING_NATIVE_FILESYSTEM,
                          status);
    }

    SetDlgItemText(hwndDlg, IDC_STATUS, status.get());
}


//
//  FUNCTION: UpdateCurrentURI()
//
//  PURPOSE: Updates the URL address field
//
void WebBrowserChromeUI::UpdateCurrentURI(nsIWebBrowserChrome *aChrome)
{
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    aChrome->GetWebBrowser(getter_AddRefs(webBrowser));
    webNavigation = do_QueryInterface(webBrowser);

    nsCOMPtr<nsIURI> currentURI;
    webNavigation->GetCurrentURI(getter_AddRefs(currentURI));
    if (currentURI)
    {
        nsCString uriString;
        currentURI->GetAsciiSpec(uriString);
        HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
        SetDlgItemText(hwndDlg, IDC_ADDRESS, uriString.get());
    }
}


//
//  FUNCTION: UpdateBusyState()
//
//  PURPOSE: Refreshes the stop/go buttons in the browser dialog
//
void WebBrowserChromeUI::UpdateBusyState(nsIWebBrowserChrome *aChrome, bool aBusy)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    HWND button;
    button = GetDlgItem(hwndDlg, IDC_STOP);
    if (button)
        EnableWindow(button, aBusy);
    button = GetDlgItem(hwndDlg, IDC_GO);
    if (button)
        EnableWindow(button, !aBusy);
    UpdateUI(aChrome);
}


//
//  FUNCTION: UpdateProgress()
//
//  PURPOSE: Refreshes the progress bar in the browser dialog
//
void WebBrowserChromeUI::UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    HWND hwndProgress = GetDlgItem(hwndDlg, IDC_PROGRESS);
    if (aCurrent < 0)
    {
        aCurrent = 0;
    }
    if (aCurrent > aMax)
    {
        aMax = aCurrent + 20; // What to do?
    }
    if (hwndProgress)
    {
        SendMessage(hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, aMax));
        SendMessage(hwndProgress, PBM_SETPOS, aCurrent, 0);
    }
}

//
//  FUNCTION: ShowContextMenu()
//
//  PURPOSE: Display a context menu for the given node
//
void WebBrowserChromeUI::ShowContextMenu(nsIWebBrowserChrome *aChrome, PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    // TODO code to test context flags and display a popup menu should go here
}

//
//  FUNCTION: ShowTooltip()
//
//  PURPOSE: Show a tooltip
//
void WebBrowserChromeUI::ShowTooltip(nsIWebBrowserChrome *aChrome, PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    // TODO code to show a tooltip should go here
}

//
//  FUNCTION: HideTooltip()
//
//  PURPOSE: Hide the tooltip
//
void WebBrowserChromeUI::HideTooltip(nsIWebBrowserChrome *aChrome)
{
    // TODO code to hide a tooltip should go here
}

void WebBrowserChromeUI::ShowWindow(nsIWebBrowserChrome *aChrome, bool aShow)
{
  HWND win = GetBrowserDlgFromChrome(aChrome);
  ::ShowWindow(win, aShow ? SW_RESTORE : SW_HIDE);
}

void WebBrowserChromeUI::SizeTo(nsIWebBrowserChrome *aChrome, PRInt32 aWidth, PRInt32 aHeight)
{
  HWND hchrome = GetBrowserDlgFromChrome(aChrome);
  HWND hbrowser = GetBrowserFromChrome(aChrome);
  RECT chromeRect, browserRect;

  ::GetWindowRect(hchrome,  &chromeRect);
  ::GetWindowRect(hbrowser, &browserRect);

  PRInt32 decoration_x = (browserRect.left - chromeRect.left) + 
                         (chromeRect.right - browserRect.right);
  PRInt32 decoration_y = (browserRect.top - chromeRect.top) + 
                         (chromeRect.bottom - browserRect.bottom);

  ::MoveWindow(hchrome, chromeRect.left, chromeRect.top,
      aWidth+decoration_x,
      aHeight+decoration_y, TRUE);
}

//
//  FUNCTION: GetResourceStringByID()
//
//  PURPOSE: Get the resource string for the ID
//
void WebBrowserChromeUI::GetResourceStringById(PRInt32 aID, char ** aReturn)
{
    char resBuf[MAX_LOADSTRING];
    int retval = LoadString( ghInstanceApp, aID, (LPTSTR)resBuf, sizeof(resBuf) );
    if (retval != 0)
    {
        size_t resLen = strlen(resBuf);
        *aReturn = (char *)calloc(resLen+1, sizeof(char *));
        if (!*aReturn) return;
            strncpy(*aReturn, resBuf, resLen);
    }
    return;
}

//-----------------------------------------------------------------------------
// AppCallbacks
//-----------------------------------------------------------------------------

nsresult AppCallbacks::CreateBrowserWindow(PRUint32 aChromeFlags,
           nsIWebBrowserChrome *aParent,
           nsIWebBrowserChrome **aNewWindow)
{
  WebBrowserChrome * chrome = new WebBrowserChrome();
  if (!chrome)
    return NS_ERROR_FAILURE;

  // the interface to return and one addref, which we assume will be
  // immediately released
  CallQueryInterface(static_cast<nsIWebBrowserChrome*>(chrome), aNewWindow);
  // now an extra addref; the window owns itself (to be released by
  // WebBrowserChromeUI::Destroy)
  NS_ADDREF(*aNewWindow);

  chrome->SetChromeFlags(aChromeFlags);
  chrome->SetParent(aParent);

  // Insert the browser
  nsCOMPtr<nsIWebBrowser> newBrowser;
  chrome->CreateBrowser(-1, -1, -1, -1, getter_AddRefs(newBrowser));
  if (!newBrowser)
    return NS_ERROR_FAILURE;

  // Place it where we want it.
  ResizeEmbedding(static_cast<nsIWebBrowserChrome*>(chrome));

  // if opened as chrome, it'll be made visible after the chrome has loaded.
  // otherwise, go ahead and show it now.
  if (!(aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME))
    WebBrowserChromeUI::ShowWindow(*aNewWindow, PR_TRUE);

  return NS_OK;
}

void AppCallbacks::EnableChromeWindow(nsIWebBrowserChrome *aWindow,
                      bool aEnabled)
{
  HWND hwnd = GetBrowserDlgFromChrome(aWindow);
  ::EnableWindow(hwnd, aEnabled ? TRUE : FALSE);
}

PRUint32 AppCallbacks::RunEventLoop(bool &aRunCondition)
{
  MSG msg;
  HANDLE hFakeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

  while (aRunCondition ) {
    // Process pending messages
    while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
      if (!::GetMessage(&msg, NULL, 0, 0)) {
        // WM_QUIT
        aRunCondition = PR_FALSE;
        break;
      }

      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // Do idle stuff
    ::MsgWaitForMultipleObjects(1, &hFakeEvent, FALSE, 100, QS_ALLEVENTS);
  }
  ::CloseHandle(hFakeEvent);
  return (PRUint32)msg.wParam;
}
