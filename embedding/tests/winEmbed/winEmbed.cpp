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
 *  Doug Turner <dougt@netscape.com> 
 */

#include <stdio.h>
#include "stdafx.h"

// Win32 header files
#include "windows.h"
#include "commctrl.h"
#include "commdlg.h"

// Mozilla header files
#include "nsEmbedAPI.h"
#include "nsIClipboardCommands.h"
#include "nsXPIDLString.h"
#include "nsIWebBrowserPersist.h"

// Local header files
#include "WebBrowserChrome.h"
#include "resource.h"

#define MAX_LOADSTRING 100

const TCHAR *szWindowClass = _T("WINEMBED");

// Foward declarations of functions included in this code module:
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static LRESULT CALLBACK BrowserWndProc(HWND, UINT, WPARAM, LPARAM);
static BOOL    CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK GetURI(HWND, UINT, WPARAM, LPARAM);
static nsresult OpenWebPage(const char * url);
static nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome);

// Global variables
static char gLastURI[100];
static UINT gDialogCount = 0;
static PRBool gDumbBanner = PR_TRUE;
static PRInt32 gDumbBannerSize = 32;
static HINSTANCE ghInstanceResources = NULL;
static HINSTANCE ghInstanceApp = NULL;

int main(int argc, char *argv[])
{
    printf("\nYou are embedded, man!\n\n");
    
    // Sophisticated command-line parsing in action
    char *szFirstURL = "http://www.mozilla.org/projects/embedding";
    if (argc > 1)
    {
        szFirstURL = argv[1];
    }

    ghInstanceApp = GetModuleHandle(NULL);
    ghInstanceResources = GetModuleHandle(NULL);

	// Initialize global strings
    TCHAR szTitle[MAX_LOADSTRING];
	LoadString(ghInstanceResources, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	MyRegisterClass(ghInstanceApp);

    // Init Embedding APIs
    NS_InitEmbedding(nsnull, nsnull);

    // Open the initial browser window
    OpenWebPage(szFirstURL);

	// Main message loop:
    MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
	    TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    // Close down Embedding APIs
    NS_TermEmbedding();

	return msg.wParam;
}


class Win32ChromeUI : public WebBrowserChromeUI
{
    NS_DECL_WEBBROWSERCHROMEUI;
};


//
//  FUNCTION: OpenWebPage()
//
//  PURPOSE: Opens a new browser dialog and starts it loading to the
//           specified url.
//
nsresult OpenWebPage(const char *url)
{
    WebBrowserChrome * chrome = new WebBrowserChrome();
    if (!chrome)
        return NS_ERROR_FAILURE;
    NS_ADDREF(chrome); // native window will hold the addref.

    // Note, the chrome owns the UI object once set & will delete it
    chrome->SetUI(new Win32ChromeUI);
    
    // Create the browser window
    nsCOMPtr<nsIWebBrowser> newBrowser;
    chrome->CreateBrowserWindow(0, -1, -1, -1, -1, getter_AddRefs(newBrowser));
    if (!newBrowser)
        return NS_ERROR_FAILURE;

    // Place it where we want it.
    ResizeEmbedding(NS_STATIC_CAST(nsIWebBrowserChrome*, chrome));

    // Start loading a page
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(newBrowser));
    return webNav->LoadURI(NS_ConvertASCIItoUCS2(url).GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
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
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(aChrome);
    HWND hwnd = NULL;
    baseWindow->GetParentNativeWindow((void**)&hwnd);
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
//  FUNCTION: SaveWebPage()
//
//  PURPOSE: Saves the contents of the web page to a file
//
void SaveWebPage(nsIWebBrowser *aWebBrowser)
{
    // Use the browser window title as the initial file name
    nsCOMPtr<nsIBaseWindow> webBrowserAsWin = do_QueryInterface(aWebBrowser);
	nsXPIDLString windowTitle;
	webBrowserAsWin->GetTitle(getter_Copies(windowTitle));
    nsCString fileName; fileName.AssignWithConversion(windowTitle);

	// Sanitize the title of all illegal characters
    fileName.CompressWhitespace();     // Remove whitespace from the ends
    fileName.StripChars("\\*|:\"><?"); // Strip illegal characters
    fileName.ReplaceChar('.', L'_');   // Dots become underscores
    fileName.ReplaceChar('/', L'-');   // Forward slashes become hyphens

    // Copy filename to a character buffer
	char szFile[_MAX_PATH];
    memset(szFile, 0, sizeof(szFile));
    fileName.ToCString(szFile, sizeof(szFile) - 1);

    // Initialize the file save as information structure
    OPENFILENAME saveFileNameInfo;
    memset(&saveFileNameInfo, 0, sizeof(saveFileNameInfo));
	saveFileNameInfo.lStructSize = sizeof(saveFileNameInfo);
	saveFileNameInfo.hwndOwner = NULL;
	saveFileNameInfo.hInstance = NULL;
	saveFileNameInfo.lpstrFilter =
        "Web Page, HTML Only (*.htm;*.html)\0*.htm;*.html\0"
        "Web Page, Complete (*.htm;*.html)\0*.htm;*.html\0"
        "Text File (*.txt)\0*.txt\0"; 
	saveFileNameInfo.lpstrCustomFilter = NULL; 
	saveFileNameInfo.nMaxCustFilter = NULL; 
	saveFileNameInfo.nFilterIndex = 1; 
	saveFileNameInfo.lpstrFile = szFile; 
	saveFileNameInfo.nMaxFile = sizeof(szFile); 
	saveFileNameInfo.lpstrFileTitle = NULL;
	saveFileNameInfo.nMaxFileTitle = 0; 
	saveFileNameInfo.lpstrInitialDir = NULL; 
	saveFileNameInfo.lpstrTitle = NULL; 
	saveFileNameInfo.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT; 
	saveFileNameInfo.nFileOffset = NULL; 
	saveFileNameInfo.nFileExtension = NULL; 
	saveFileNameInfo.lpstrDefExt = "htm"; 
	saveFileNameInfo.lCustData = NULL; 
	saveFileNameInfo.lpfnHook = NULL; 
	saveFileNameInfo.lpTemplateName = NULL; 

	if (GetSaveFileName(&saveFileNameInfo))
	{
        // Does the user want to save the complete document including
        // all frames, images, scripts, stylesheets etc. ?
        char *pszDataPath = NULL;
        if (saveFileNameInfo.nFilterIndex == 2) // 2nd choice means save everything
        {
            static char szDataFile[_MAX_PATH];
            char szDataPath[_MAX_PATH];
            char drive[_MAX_DRIVE];
            char dir[_MAX_DIR];
            char fname[_MAX_FNAME];
            char ext[_MAX_EXT];

            _splitpath(szFile, drive, dir, fname, ext);
            sprintf(szDataFile, "%s_files", fname);
            _makepath(szDataPath, drive, dir, szDataFile, "");

            pszDataPath = szDataPath;
       }

        // Save away
        nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(aWebBrowser));
        persist->SaveDocument(nsnull, szFile, pszDataPath);
	}
}


//
//  FUNCTION: ResizeEmbedding()
//
//  PURPOSE: Resizes the webbrowser window to fit it's container.
//
nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome)
{
    if (!chrome)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(chrome);
    
    HWND hWnd;
    baseWindow->GetParentNativeWindow((void**)&hWnd);
    
    if (!hWnd)
        return NS_ERROR_NULL_POINTER;
    
    RECT rect;
    GetClientRect(hWnd, &rect);
    
    // Add space for the banner if there is enough to show it
    if (gDumbBanner && rect.bottom - rect.top > gDumbBannerSize)
    {
        rect.top += gDumbBannerSize;
    }

    baseWindow->SetPositionAndSize(rect.left, 
                                   rect.top, 
                                   rect.right - rect.left, 
                                   rect.bottom - rect.top,
                                   PR_TRUE);
    
    baseWindow->SetVisibility(PR_TRUE);
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

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC) BrowserWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(ghInstanceResources, (LPCTSTR)IDI_WINEMBED);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(ghInstanceResources, (LPCTSTR)IDI_SMALL);

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

    PRBool canGoBack = PR_FALSE;
    PRBool canGoForward = PR_FALSE;
    if (webNavigation)
    {
        webNavigation->GetCanGoBack(&canGoBack);
        webNavigation->GetCanGoForward(&canGoForward);
    }

    PRBool canCutSelection = PR_FALSE;
    PRBool canCopySelection = PR_FALSE;
    PRBool canPasteSelection = PR_FALSE;

    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
    if (nsIClipboardCommands)
    {
        clipCmds->CanCutSelection(&canCutSelection);
        clipCmds->CanCopySelection(&canCopySelection);
        clipCmds->CanPasteSelection(&canPasteSelection);
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
                ((canPasteSelection) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
    }

    EnableWindow(GetDlgItem(hwndDlg, IDC_BACK), canGoBack);
    EnableWindow(GetDlgItem(hwndDlg, IDC_FORWARD), canGoForward);
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
BOOL CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Get the browser and other pointers since they are used a lot below
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    nsIWebBrowserChrome *chrome = nsnull;
    if (hwndBrowser)
    {
        chrome = (nsIWebBrowserChrome *) GetWindowLong(hwndBrowser, GWL_USERDATA);
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
    case WM_INITMENU:
        UpdateUI(chrome);
        return TRUE;

    case WM_NOTIFY:

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
                GetDlgItemText(hwndDlg, IDC_ADDRESS, szURL, sizeof(szURL) / sizeof(szURL[0]) - 1);
                webNavigation->LoadURI(NS_ConvertASCIItoUCS2(szURL).GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
            }
            break;

        case IDC_STOP:
            webNavigation->Stop();
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
            gLastURI[0] = 0;
            if (DialogBox(ghInstanceResources, (LPCTSTR)MOZ_GetURI, hwndDlg, (DLGPROC)GetURI) == IDOK)
            {
                OpenWebPage(gLastURI);
            }
            break;

        case MOZ_Save:
            SaveWebPage(webBrowser);
            break;

        case MOZ_Print:
            {
                // NOTE: Embedding code shouldn't need to get the docshell or
                //       contentviewer AT ALL. This code below will break one
                //       day but will have to do until the embedding API has
                //       a cleaner way to do the same thing.

                nsCOMPtr <nsIDocShell> rootDocShell = do_GetInterface(webBrowser);
                nsCOMPtr<nsIContentViewer> pContentViewer;
                nsresult res = rootDocShell->GetContentViewer(getter_AddRefs(pContentViewer));

                if (NS_SUCCEEDED(res))
                {
                    nsCOMPtr<nsIContentViewerFile> spContentViewerFile = do_QueryInterface(pContentViewer); 
                    spContentViewerFile->Print(PR_TRUE, nsnull);
                }
            }
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
                clipCmds->PasteSelection();
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
                LoadString(ghInstanceResources, IDS_ABOUT_TITLE, szAboutTitle, MAX_LOADSTRING);
                LoadString(ghInstanceResources, IDS_ABOUT, szAbout, MAX_LOADSTRING);
                MessageBox(NULL, szAbout, szAboutTitle, MB_OK);
            }
            break;
		}

	    return TRUE;

    case WM_SIZE:
        {
            UINT newDlgWidth = LOWORD(lParam);
            UINT newDlgHeight = HIWORD(lParam);

            // TODO Reposition the control bar - for the moment it's fixed size

            // Reposition the status area. Status bar
            // gets any space that the fixed size progress bar doesn't use.
            RECT rcStatus;
            RECT rcProgress;
            HWND hwndStatus = GetDlgItem(hwndDlg, IDC_STATUS);
            HWND hwndProgress = GetDlgItem(hwndDlg, IDC_PROGRESS);
            GetWindowRect(hwndStatus, &rcStatus);
            GetWindowRect(hwndProgress, &rcProgress);
            int progressWidth = rcProgress.right - rcProgress.left;
            int statusWidth = newDlgWidth - progressWidth;
            int statusHeight = rcStatus.bottom - rcStatus.top;
            SetWindowPos(hwndStatus,
                         HWND_TOP,
                         0, newDlgHeight - statusHeight,
                         statusWidth,
                         statusHeight,
                         SWP_NOZORDER);
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

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            DestroyWindow(hwndDlg);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        gDialogCount--;
        if (webNavigation)
        {
            webNavigation->Stop();
        }
        // Quit when there are no more browser windows
        if (gDialogCount == 0)
        {
            PostQuitMessage(0);
        }
	    return TRUE;
    }
    return FALSE;
}


//
//  FUNCTION: BrowserWndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the browser container window.
//
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- Cleanup
//
//
LRESULT CALLBACK BrowserWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR szHello[MAX_LOADSTRING];
	LoadString(ghInstanceResources, IDS_HELLO, szHello, MAX_LOADSTRING);
    nsIWebBrowserChrome *chrome = (nsIWebBrowserChrome *) GetWindowLong(hWnd, GWL_USERDATA);

	switch (message) 
	{
    case WM_SIZE:
        // Resize the embedded browser
        ResizeEmbedding(chrome);
        break;
        
    case WM_ERASEBKGND:
        // Reduce flicker by not painting the non-visible background
        return 1;

	case WM_PAINT:
        // Draw a banner message above the browser
        if (gDumbBanner)
        {
            // this draws that silly text at the top of the window.
            hdc = BeginPaint(hWnd, &ps);
		    RECT rc;
		    GetClientRect(hWnd, &rc);
            // Only draw banner if there is space
            if (rc.bottom - rc.top > gDumbBannerSize)
            {
                rc.bottom = gDumbBannerSize;
                FillRect(hdc, &rc, (HBRUSH) GetStockObject(WHITE_BRUSH));
                FrameRect(hdc,  &rc, CreateSolidBrush( 0x00 ) );
                rc.top = 4;
		        DrawText(hdc, szHello, strlen(szHello), &rc, DT_CENTER);
            }
            EndPaint(hWnd, &ps);
        }
		break;

	case WM_DESTROY:
        NS_RELEASE(chrome);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}


//
//  FUNCTION: GetURI(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Dialog handler procedure for the open uri dialog.
//
LRESULT CALLBACK GetURI(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
			return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
        {
            GetDlgItemText(hDlg, MOZ_EDIT_URI, gLastURI, 100);
        }
        EndDialog(hDlg, LOWORD(wParam));
        return TRUE;
	}

    return FALSE;
}


///////////////////////////////////////////////////////////////////////////////
// Win32ChromeUI

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
nativeWindow Win32ChromeUI::CreateNativeWindow(nsIWebBrowserChrome* chrome)
{
    // Load the browser dialog from resource
    HWND hwndDialog = CreateDialog(ghInstanceResources,
                                   MAKEINTRESOURCE(IDD_BROWSER),
                                   NULL,
                                   BrowserDlgProc);
    if (!hwndDialog)
    {
        return NULL;
    }

    gDialogCount++;

    // Stick a menu onto it
    HMENU hmenuDlg = LoadMenu(ghInstanceResources, MAKEINTRESOURCE(IDC_WINEMBED));
    SetMenu(hwndDialog, hmenuDlg);

    // Add some interesting URLs to the address drop down
    HWND hwndAddress = GetDlgItem(hwndDialog, IDC_ADDRESS);
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.mozilla.org/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.netscape.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://127.0.0.1/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.yahoo.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.travelocity.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.disney.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.go.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.google.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.ebay.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.shockwave.com/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.slashdot.org/"));
    SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) _T("http://www.quicken.com/"));

    // Fetch the browser window handle
    HWND hwndBrowser = GetDlgItem(hwndDialog, IDC_BROWSER);
    SetWindowLong(hwndBrowser, GWL_USERDATA, (LONG)chrome);  // save the browser LONG_PTR.
    SetWindowLong(hwndBrowser, GWL_STYLE, GetWindowLong(hwndBrowser, GWL_STYLE) | WS_CLIPCHILDREN);
    return hwndBrowser;
}


//
//  FUNCTION: UpdateStatusBarText()
//
//  PURPOSE: Set the status bar text.
//
void Win32ChromeUI::UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    nsCString status; 
    if (aStatusText)
        status.AssignWithConversion(aStatusText);
    SetDlgItemText(hwndDlg, IDC_STATUS, status.GetBuffer());
}


//
//  FUNCTION: UpdateCurrentURI()
//
//  PURPOSE: Updates the URL address field
//
void Win32ChromeUI::UpdateCurrentURI(nsIWebBrowserChrome *aChrome)
{
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    aChrome->GetWebBrowser(getter_AddRefs(webBrowser));
    webNavigation = do_QueryInterface(webBrowser);

    nsCOMPtr<nsIURI> currentURI;
    webNavigation->GetCurrentURI(getter_AddRefs(currentURI));
    if (currentURI)
    {
        nsXPIDLCString uriString;
        currentURI->GetSpec(getter_Copies(uriString));
        HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
        SetDlgItemText(hwndDlg, IDC_ADDRESS, uriString.get());
    }
}


//
//  FUNCTION: UpdateBusyState()
//
//  PURPOSE: Refreshes the stop/go buttons in the browser dialog
//
void Win32ChromeUI::UpdateBusyState(nsIWebBrowserChrome *aChrome, PRBool aBusy)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), aBusy);
    EnableWindow(GetDlgItem(hwndDlg, IDC_GO), !aBusy);
    UpdateUI(aChrome);
}


//
//  FUNCTION: UpdateProgress()
//
//  PURPOSE: Refreshes the progress bar in the browser dialog
//
void Win32ChromeUI::UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax)
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
    SendMessage(hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, aMax));
    SendMessage(hwndProgress, PBM_SETPOS, aCurrent, 0);
}



