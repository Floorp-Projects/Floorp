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
 * Contributor(s): Radha Kulkarni <radha@netscape.com> 
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include "stdafx.h"

// Win32 header files
#include "windows.h"
#include "commctrl.h"
#include "commdlg.h"

// Mozilla header files
#include "nsCOMPtr.h"
#include "nsEmbedAPI.h"
#include "nsWeakReference.h"
#include "nsIClipboardCommands.h"
#include "nsXPIDLString.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWindowWatcher.h"
#include "nsIProfile.h"
//#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIProfileChangeStatus.h"
#include "nsIURI.h"
#include "plstr.h"
#include "nsIInterfaceRequestor.h"
#include "nsCRT.h"
#include "nsString.h"

#include "nsIWebBrowser.h"
#include "nsIComponentManager.h"
#include "nsIServiceManagerUtils.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIComponentRegistrar.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

// Local header files
//#include "nsIQABrowserChrome.h"
#include "mozEmbed.h"
#include "nsQAWindowCreator.h"
#include "resource.h"
#include "nsQABrowserCID.h"
#include "nsQABrowserUIGlue.h"
//#include "nsIQABrowserView.h"


// Printing header files
#include "nsIPrintSettings.h"
#include "nsIWebBrowserPrint.h"


#define MAX_LOADSTRING 100
#define MAX_BROWSER_ALLOWED 50

const TCHAR *szWindowClass = _T("MOZEMBED");

// Forward declarations of functions included in this code module:
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static LRESULT CALLBACK BrowserWndProc(HWND, UINT, WPARAM, LPARAM);
static BOOL    CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static nsresult InitializeWindowCreator();
static nsresult OpenWebPage(const char * url);
static nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome);
// Profile chooser stuff
static BOOL ChooseNewProfile(BOOL bShowForMultipleProfilesOnly, const char *szDefaultProfile);
static LRESULT CALLBACK ChooseProfileDlgProc(HWND, UINT, WPARAM, LPARAM);
extern nsresult RegisterComponents();

// Global variables
static UINT gDialogCount = 0;
static BOOL gProfileSwitch = FALSE;
static HINSTANCE ghInstanceResources = NULL;
static HINSTANCE ghInstanceApp = NULL;
static char gFirstURL[1024];

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
    PRBool canPaste = PR_FALSE;

    nsCOMPtr<nsIClipboardCommands> clipCmds(do_GetInterface(webBrowser));
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




////////////////////////////////////////////////////////////////////////////////////////////
// Main program
////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
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
  char *szFirstURL = "http://www.mozilla.org/projects/embedding";
	char *szDefaultProfile = nsnull;
	int argn;
	for (argn = 1; argn < argc; argn++)
	{
		if (stricmp("-P", argv[argn]) == 0)
		{
			if (argn + 1 < argc)
			{
				szDefaultProfile = argv[++argn];
			}
		}
		else
		{
	        szFirstURL = argv[argn];
		}
    }
	strncpy(gFirstURL, szFirstURL, sizeof(gFirstURL) - 1);

  ghInstanceApp = GetModuleHandle(NULL);
  ghInstanceResources = GetModuleHandle(NULL);

	// Initialize global strings
    TCHAR szTitle[MAX_LOADSTRING];
	LoadString(ghInstanceResources, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	MyRegisterClass(ghInstanceApp);

  // Init Embedding APIs
  NS_InitEmbedding(nsnull, nsnull);
  RegisterComponents();

	// Choose the new profile
  // Have to do this to initialise global history
	if (!ChooseNewProfile(TRUE, szDefaultProfile))
    {
        NS_TermEmbedding();
        return 1;
    }

  WPARAM rv;
  {   
    InitializeWindowCreator();

    // Open the initial browser window
    OpenWebPage(gFirstURL);

	  // Main message loop.
    // NOTE: We use a fake event and a timeout in order to process idle stuff for
    //       Mozilla every 1/10th of a second.
    PRBool runCondition = PR_TRUE;

    rv = AppCallbacks::RunEventLoop(runCondition);

    // observer->Release();
  }
  // Close down Embedding APIs
  NS_TermEmbedding();

  return rv;
}


/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This is how all new windows are opened,
   except any created directly by the embedding app. */
nsresult InitializeWindowCreator()
{

    printf("In InitializeWindowCreator\n");
    // create an nsWindowCreator and give it to the WindowWatcher service
    WindowCreator *creatorCallback = new WindowCreator();
    if (creatorCallback)
    {
        nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator *, creatorCallback));
        if (windowCreator)
        {
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
            if (wwatch)
            {
                wwatch->SetWindowCreator(windowCreator);
                return NS_OK;
            }
        }
    }
    return NS_ERROR_FAILURE;
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
  printf("In OpenWebpage\n");
  
  if (gDialogCount == MAX_BROWSER_ALLOWED)
    return NS_ERROR_FAILURE;

  // Create the UI Glue object. This is the glue between nsIWebBrowserChrome and
  // the native UI code.
  nsCOMPtr<nsIQABrowserUIGlue> browserUIGlue;
  browserUIGlue = do_CreateInstance(NS_QABROWSERUIGLUE_CONTRACTID, &rv);
 
  if (!browserUIGlue)
    return NS_ERROR_FAILURE;
  
  // Create a new browser  window
  nsIWebBrowserChrome *  chrome;
  rv = browserUIGlue->CreateNewBrowserWindow(nsIWebBrowserChrome::CHROME_ALL, nsnull,
                                        &chrome);
  if (NS_SUCCEEDED(rv)) {
    browserUIGlue->LoadURL(url);
  }
  return rv;

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
  PL_strncpyz(szFile, fileName.get(), sizeof(szFile) - 1); // XXXldb probably should be just sizeof(szfile)

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

     nsCOMPtr<nsILocalFile> file;
     NS_NewNativeLocalFile(nsDependentCString(szFile), TRUE, getter_AddRefs(file));

     nsCOMPtr<nsILocalFile> dataPath;
     if (pszDataPath)
     {
        NS_NewNativeLocalFile(nsDependentCString(pszDataPath), TRUE, getter_AddRefs(dataPath));
     }

     persist->SaveDocument(nsnull, file, dataPath, nsnull, 0, 0);
  }
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

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC) BrowserWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(ghInstanceResources, (LPCTSTR)IDI_MOZEMBED);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(ghInstanceResources, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
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
	if (uMsg == WM_COMMAND && LOWORD(wParam) == MOZ_SwitchProfile)
	{
        ChooseNewProfile(FALSE, NULL);
		return FALSE;
	}

    // Get the browser and other pointers since they are used a lot below
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    nsIWebBrowserChrome *chrome = nsnull ;
    if (hwndBrowser)
    {
        chrome = (nsIWebBrowserChrome *) GetWindowLong(hwndBrowser, GWL_USERDATA);
    }
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
    if (chrome)
    {
        chrome->GetWebBrowser(getter_AddRefs(webBrowser));
        webNavigation = do_QueryInterface(webBrowser);
        webBrowserPrint = do_GetInterface(webBrowser);
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
          if (!webNavigation || !webBrowser || !chrome)
            return FALSE;
          
          webNavigation->Stop(nsIWebNavigation::STOP_ALL);
          chrome->ExitModalEventLoop(NS_OK);

          // Explicitly destroy the embedded browser and then the chrome
          // First the browser
          nsCOMPtr<nsIBaseWindow> browserAsWin = do_QueryInterface(webBrowser);
          if (browserAsWin)
            browserAsWin->Destroy();

          // Now the chrome
          chrome->SetWebBrowser(nsnull);
          NS_RELEASE(chrome);
         
          return FALSE;        
        }
        break;
    case WM_DESTROY:
	    return FALSE;

    case WM_COMMAND: 
      {
        if (!webBrowser)
        {
            return FALSE;
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
                    NS_ConvertASCIItoUCS2(szURL).get(),
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

        case MOZ_Save:
            SaveWebPage(webBrowser);
            break;

        case MOZ_Print:
            {
                // NOTE: Embedding code shouldn't need to get the docshell or
                //       contentviewer AT ALL. This code below will break one
                //       day but will have to do until the embedding API has
                //       a cleaner way to do the same thing.
              if (webBrowserPrint)
              {
                  nsCOMPtr<nsIPrintSettings> printSettings;
                  webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
                  NS_ASSERTION(printSettings, "You can't PrintPreview without a PrintSettings!");
                  if (printSettings) 
                  {
                      printSettings->SetPrintSilent(PR_TRUE);
                      webBrowserPrint->Print(printSettings, (nsIWebProgressListener*)nsnull);
                  }
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
                LoadString(ghInstanceResources, IDS_ABOUT_TITLE, szAboutTitle, MAX_LOADSTRING);
                LoadString(ghInstanceResources, IDS_ABOUT, szAbout, MAX_LOADSTRING);
                MessageBox(NULL, szAbout, szAboutTitle, MB_OK);
            }
            break;
          }

	    return TRUE;
      }

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
//  FUNCTION: BrowserWndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the browser container window.
//
LRESULT CALLBACK BrowserWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    nsIWebBrowserChrome *chrome = (nsIWebBrowserChrome *) GetWindowLong(hWnd, GWL_USERDATA);
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


///////////////////////////////////////////////////////////////////////////////
// Profile chooser dialog


//
//  FUNCTION: ChooseNewProfile()
//
//  PURPOSE: Allows the user to select a new profile from a list.
//           The bShowForMultipleProfilesOnly argument specifies whether the
//           function should automatically select the first profile and return
//           without displaying a dialog box if there is only one profile to
//           select.
//
BOOL ChooseNewProfile(BOOL bShowForMultipleProfilesOnly, const char *szDefaultProfile)
{
    nsresult rv;
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
        return FALSE;
    }

    if (szDefaultProfile)
    {
        // Make a new default profile
        nsAutoString newProfileName; newProfileName.AssignWithConversion(szDefaultProfile);
        rv = profileService->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_FALSE);
        if (NS_FAILED(rv)) return FALSE;
        rv = profileService->SetCurrentProfile(newProfileName.get());
        if (NS_FAILED(rv)) return FALSE;
        return TRUE;
    }

    PRInt32 profileCount = 0;
    rv = profileService->GetProfileCount(&profileCount);
    if (profileCount == 0)
    {
        // Make a new default profile
        NS_NAMED_LITERAL_STRING(newProfileName, "mozEmbed");
        rv = profileService->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_FALSE);
        if (NS_FAILED(rv)) return FALSE;
        rv = profileService->SetCurrentProfile(newProfileName.get());
        if (NS_FAILED(rv)) return FALSE;
        return TRUE;
    }
    else if (profileCount == 1 && bShowForMultipleProfilesOnly)
    {
        // GetCurrentProfile returns the profile which was last used but is not nescesarily
        // active. Call SetCurrentProfile to make it installed and active.
        
        nsXPIDLString   currProfileName;
        rv = profileService->GetCurrentProfile(getter_Copies(currProfileName));
        if (NS_FAILED(rv)) return FALSE;
        rv = profileService->SetCurrentProfile(currProfileName);
        if (NS_FAILED(rv)) return FALSE;
        return TRUE;
    }

    INT nResult;
    nResult = DialogBox(ghInstanceResources, (LPCTSTR)IDD_CHOOSEPROFILE, NULL, (DLGPROC)ChooseProfileDlgProc);
    return (nResult == IDOK) ? TRUE : FALSE;
}


//
//  FUNCTION: ChooseProfileDlgProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Dialog handler procedure for the open uri dialog.
//
LRESULT CALLBACK ChooseProfileDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  nsresult rv;
	switch (message)
	{
	case WM_INITDIALOG:
        {
            HWND hwndProfileList = GetDlgItem(hDlg, IDC_PROFILELIST);

            nsCOMPtr<nsIProfile> profileService = 
                     do_GetService(NS_PROFILE_CONTRACTID, &rv);

            // Get the list of profile names and add them to the list box
            PRUint32 listLen = 0;
            PRUnichar **profileList = nsnull;
            rv = profileService->GetProfileList(&listLen, &profileList);
            for (PRUint32 index = 0; index < listLen; index++)
            {
#ifdef UNICODE
                SendMessageW(hwndProfileList, LB_ADDSTRING, 0, (LPARAM) profileList[index]);
#else
                nsCAutoString profile; profile.AssignWithConversion(profileList[index]);
                SendMessageA(hwndProfileList, LB_ADDSTRING, 0, (LPARAM) profile.get());
#endif
            }

            // Select the current profile (if there is one)

            // Get the current profile
#ifdef UNICODE
            nsXPIDLString currProfile;
            profileService->GetCurrentProfile(getter_Copies(currProfile));
#else
            nsXPIDLString currProfileUnicode;
            profileService->GetCurrentProfile(getter_Copies(currProfileUnicode));
            nsCAutoString currProfile; currProfile.AssignWithConversion(currProfileUnicode);
#endif

            // Now find and select it
            INT currentProfileIndex = LB_ERR;
            currentProfileIndex = SendMessage(hwndProfileList, LB_FINDSTRINGEXACT, -1, (LPARAM) currProfile.get());
            if (currentProfileIndex != LB_ERR)
            {
                SendMessage(hwndProfileList, LB_SETCURSEL, currentProfileIndex, 0);
            }
        }
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK ||
			(HIWORD(wParam) & LBN_DBLCLK && LOWORD(wParam) == IDC_PROFILELIST))
        {
            HWND hwndProfileList = GetDlgItem(hDlg, IDC_PROFILELIST);

            // Get the selected profile from the list box and make it current
            INT currentProfileIndex = SendMessage(hwndProfileList, LB_GETCURSEL, 0, 0);
            if (currentProfileIndex != LB_ERR)
            {
                nsCOMPtr<nsIProfile> profileService = 
                         do_GetService(NS_PROFILE_CONTRACTID, &rv);
                // Convert TCHAR name to unicode and make it current
                INT profileNameLen = SendMessage(hwndProfileList, LB_GETTEXTLEN, currentProfileIndex, 0);
                TCHAR *profileName = new TCHAR[profileNameLen + 1];
                SendMessage(hwndProfileList, LB_GETTEXT, currentProfileIndex, (LPARAM) profileName);
                nsAutoString newProfile; newProfile.AssignWithConversion(profileName);
                rv = profileService->SetCurrentProfile(newProfile.get());
            }
	        EndDialog(hDlg, IDOK);
        }
		else if (LOWORD(wParam) == IDCANCEL)
		{
	        EndDialog(hDlg, LOWORD(wParam));
		}
        return TRUE;
	}

    return FALSE;
}


//-----------------------------------------------------------------------------
// AppCallbacks
//-----------------------------------------------------------------------------


void AppCallbacks::EnableChromeWindow(nsIWebBrowserChrome *aWindow,
                      PRBool aEnabled)
{
  HWND hwnd = GetBrowserDlgFromChrome(aWindow);
  ::EnableWindow(hwnd, aEnabled ? TRUE : FALSE);
}

PRUint32 AppCallbacks::RunEventLoop(PRBool &aRunCondition)
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

      PRBool wasHandled = PR_FALSE;
      ::NS_HandleEmbeddingEvent(msg, wasHandled);
      if (wasHandled)
        continue;
 
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // Do idle stuff
    ::MsgWaitForMultipleObjects(1, &hFakeEvent, FALSE, 100, QS_ALLEVENTS);
  }
  ::CloseHandle(hFakeEvent);
  return msg.wParam;
}


///////////////////////////////////////////////////////////////////////////////
// nsQABrowserUIGlue
///////////////////////////////////////////////////////////////////////////////

nsQABrowserUIGlue::nsQABrowserUIGlue():mAllowNewWindows(PR_TRUE)
{
}

nsQABrowserUIGlue::~nsQABrowserUIGlue()
{

}

NS_IMPL_ISUPPORTS1(nsQABrowserUIGlue, nsIQABrowserUIGlue)

////////////////////////////////////////////////////////////////////////////////
// nsIQABrowserUIGlue
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsQABrowserUIGlue::CreateNewBrowserWindow(PRInt32 aChromeFlags,
                                          nsIWebBrowserChrome *aParent,
                                          nsIWebBrowserChrome **aNewWindow)
{

  printf("In nsQABrowserUIGlue::CreateNewBrowserWindow\n");

  nsresult rv;
  

  // Create the chrome object. This implements all the interfaces
  // that an embedding application must implement. 
  nsCOMPtr<nsIQABrowserChrome> chrome(do_CreateInstance(NS_QABROWSERCHROME_CONTRACTID, &rv));
  if (!chrome)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome(do_QueryInterface(chrome));

  if (!webBrowserChrome)
    return NS_ERROR_FAILURE;

  // now an extra addref; the window owns itself (to be released by
  // nsQABrowserUIGlue::Destroy) 
  *aNewWindow = webBrowserChrome.get();
  NS_ADDREF(*aNewWindow);
 
    // Set the chrome flags on the chrome. This may not be necessary.
  webBrowserChrome->SetChromeFlags(aChromeFlags);
  //  chrome->SetParent(aParent);

  // Create the browser view object. nsIBrowserView  creates and holds
  // handle to the nsIWebBrowser  object.   
  mBrowserView = do_CreateInstance(NS_QABROWSERVIEW_CONTRACTID, &rv);
  if (!mBrowserView)
    return NS_ERROR_FAILURE;

  // Create the native window.
  nativeWindow nativeWnd;
  nativeWnd = CreateNativeWindow(*aNewWindow);
  
  // Wire all the layers together.
  chrome->InitQAChrome(this,  nativeWnd);

    // Create the actual browser.
  mBrowserView->CreateBrowser(nativeWnd, webBrowserChrome);
 
  // Place it where we want it.
  ResizeEmbedding(webBrowserChrome);

  // if opened as chrome, it'll be made visible after the chrome has loaded.
  // otherwise, go ahead and show it now.
  if (!(aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME))
    ShowWindow(*aNewWindow, PR_TRUE);

  return NS_OK;
}


//
// FUNCTION: Destroy()
//
// PURPOSE: Destroy the window specified by the chrome
//
NS_IMETHODIMP
nsQABrowserUIGlue::Destroy(nsIWebBrowserChrome* chrome)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  nsCOMPtr<nsIWebNavigation> webNavigation;

  chrome->GetWebBrowser(getter_AddRefs(webBrowser));
  webNavigation = do_QueryInterface(webBrowser);
  if (webNavigation)
    webNavigation->Stop(nsIWebNavigation::STOP_ALL);

  chrome->ExitModalEventLoop(NS_OK);

  // Explicitly destroy the embedded browser and then the chrome
  // First the browser
  nsCOMPtr<nsIBaseWindow> browserAsWin = do_QueryInterface(webBrowser);
  if (browserAsWin)
    browserAsWin->Destroy();

      // Now the chrome
  chrome->SetWebBrowser(nsnull);
  NS_RELEASE(chrome);
  return NS_OK;
}


//
// FUNCTION: Called as the final act of a chrome object during its destructor
//
NS_IMETHODIMP
nsQABrowserUIGlue::Destroyed(nsIWebBrowserChrome* chrome)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(chrome);
	if (hwndDlg == NULL)
	{
		return NS_ERROR_FAILURE;
	}

    // Clear the window user data
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    SetWindowLong(hwndBrowser, GWL_USERDATA, nsnull);
	  DestroyWindow(hwndBrowser);
    DestroyWindow(hwndDlg);

    --gDialogCount;
    if (gDialogCount == 0)
    {
        if (gProfileSwitch)
        {
            gProfileSwitch = FALSE;
            OpenWebPage(gFirstURL);
        }
        else
        {
            // Quit when there are no more browser objects
            PostQuitMessage(0);
        }
    }
    return NS_OK;
}


//
// FUNCTION: Set the input focus onto the browser window
//
NS_IMETHODIMP
nsQABrowserUIGlue::SetFocus(nsIWebBrowserChrome *chrome)
{
  HWND hwndDlg = GetBrowserDlgFromChrome(chrome);
	if (hwndDlg == NULL)
	{
		return NS_ERROR_FAILURE;
	}
    
  HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
  ::SetFocus(hwndBrowser);
  return NS_OK;
}

//
//  FUNCTION: UpdateStatusBarText()
//
//  PURPOSE: Set the status bar text.
//
NS_IMETHODIMP
nsQABrowserUIGlue::UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    nsCString status; 
    if (aStatusText)
        status.AssignWithConversion(aStatusText);
    SetDlgItemText(hwndDlg, IDC_STATUS, status.get());
    return NS_OK;
}


//
//  FUNCTION: UpdateCurrentURI()
//
//  PURPOSE: Updates the URL address field
//
NS_IMETHODIMP
nsQABrowserUIGlue::UpdateCurrentURI(nsIWebBrowserChrome *aChrome)
{
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    aChrome->GetWebBrowser(getter_AddRefs(webBrowser));
    webNavigation = do_QueryInterface(webBrowser);

    nsCOMPtr<nsIURI> currentURI;
    webNavigation->GetCurrentURI(getter_AddRefs(currentURI));
    if (currentURI)
    {
        nsCAutoString uriString;
        currentURI->GetAsciiSpec(uriString);
        HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
        SetDlgItemText(hwndDlg, IDC_ADDRESS, uriString.get());
    }
    return NS_OK;
}


//
//  FUNCTION: UpdateBusyState()
//
//  PURPOSE: Refreshes the stop/go buttons in the browser dialog
//
NS_IMETHODIMP
nsQABrowserUIGlue::UpdateBusyState(nsIWebBrowserChrome *aChrome, PRBool aBusy)
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
    return NS_OK;
}


//
//  FUNCTION: UpdateProgress()
//
//  PURPOSE: Refreshes the progress bar in the browser dialog
//
NS_IMETHODIMP
nsQABrowserUIGlue::UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax)
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
    return NS_OK;
}

//
//  FUNCTION: ShowContextMenu()
//
//  PURPOSE: Display a context menu for the given node
//
NS_IMETHODIMP
nsQABrowserUIGlue::ShowContextMenu(nsIWebBrowserChrome *aChrome, PRInt32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    // TODO code to test context flags and display a popup menu should go here
  return NS_OK;
}

//
//  FUNCTION: ShowTooltip()
//
//  PURPOSE: Show a tooltip
//
NS_IMETHODIMP
nsQABrowserUIGlue::ShowTooltip(nsIWebBrowserChrome *aChrome, PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    // TODO code to show a tooltip should go here
  return NS_OK;
}

//
//  FUNCTION: HideTooltip()
//
//  PURPOSE: Hide the tooltip
//
NS_IMETHODIMP
nsQABrowserUIGlue::HideTooltip(nsIWebBrowserChrome *aChrome)
{
    // TODO code to hide a tooltip should go here
  return NS_OK;
}

NS_IMETHODIMP
nsQABrowserUIGlue::ShowWindow(nsIWebBrowserChrome *aChrome, PRBool aShow)
{
  HWND win = GetBrowserDlgFromChrome(aChrome);
  return ::ShowWindow(win, aShow ? SW_RESTORE : SW_HIDE);
}

NS_IMETHODIMP
nsQABrowserUIGlue::SizeTo(nsIWebBrowserChrome *aChrome, PRInt32 aWidth, PRInt32 aHeight)
{
  HWND win = GetBrowserDlgFromChrome(aChrome);
  RECT winRect;

  ::GetWindowRect(win, &winRect);
  return ::MoveWindow(win, winRect.left, winRect.top, aWidth, aHeight, TRUE);
}

//
//  FUNCTION: GetResourceStringByID()
//
//  PURPOSE: Get the resource string for the ID
//
NS_IMETHODIMP
nsQABrowserUIGlue::GetResourceStringById(PRInt32 aID, char ** aReturn)
{
    char resBuf[MAX_LOADSTRING];
    int retval = LoadString( ghInstanceResources, aID, (LPTSTR)resBuf, sizeof(resBuf) );
    if (retval != 0)
    {
        int resLen = strlen(resBuf);
        *aReturn = (char *)calloc(resLen+1, sizeof(char *));
        if (!*aReturn) return NS_OK;
            PL_strncpy(*aReturn, (char *) resBuf, resLen);
    }
    return NS_OK; 
}

NS_IMETHODIMP
nsQABrowserUIGlue::SetTitle(const PRUnichar * aTitle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsQABrowserUIGlue::GetTitle(PRUnichar ** aTitle)
{
  return NS_OK;
}


NS_IMETHODIMP
nsQABrowserUIGlue::SetVisibility(PRBool aVisibility)
{
  return NS_OK;
}


NS_IMETHODIMP
nsQABrowserUIGlue::GetVisibility(PRBool * aVisibility)
{
  *aVisibility = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsQABrowserUIGlue::SetAllowNewBrowserWindows(PRBool aValue)
{
  mAllowNewWindows = aValue;
  return NS_OK;
}


NS_IMETHODIMP
nsQABrowserUIGlue::GetAllowNewBrowserWindows(PRBool * aReturn)
{
  if (aReturn)
    *aReturn = mAllowNewWindows;
  return NS_OK;
}

NS_IMETHODIMP
nsQABrowserUIGlue::LoadHomePage()
{
  NS_NAMED_LITERAL_CSTRING (url, "http://www.mozilla.org/embedding");
  return LoadURL(url.get());

}

NS_IMETHODIMP
nsQABrowserUIGlue::LoadURL(const char * aURL)
{
  nsresult rv = NS_OK;
  // Start loading a page
  nsCOMPtr<nsIWebBrowser> newBrowser;
  mBrowserView->GetWebBrowser(getter_AddRefs(newBrowser));
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(newBrowser, &rv));
  if (webNav)
    return webNav->LoadURI(NS_ConvertASCIItoUCS2(aURL).get(),
                           nsIWebNavigation::LOAD_FLAGS_NONE,
                           nsnull,
                           nsnull,
                           nsnull);
  return rv;
}


//
//  FUNCTION: CreateNativeWindow()
//
//  PURPOSE: Creates a new browser dialog.
//  COMMENTS:
//
//    This function loads the browser dialog from a resource template
//    and returns the HWND for the webbrowser container dialog item
//    to the caller.
//
nativeWindow
nsQABrowserUIGlue::CreateNativeWindow(nsIWebBrowserChrome* chrome)
{
  // Load the browser dialog from resource
  HWND hwndDialog;
  PRUint32 chromeFlags;

  chrome->GetChromeFlags(&chromeFlags);
  if ((chromeFlags & nsIWebBrowserChrome::CHROME_ALL) == nsIWebBrowserChrome::CHROME_ALL)
    hwndDialog = CreateDialog(ghInstanceResources,
                              MAKEINTRESOURCE(IDD_BROWSER),
                              NULL,
                              BrowserDlgProc);
  else
    hwndDialog = CreateDialog(ghInstanceResources,
                              MAKEINTRESOURCE(IDD_BROWSER_NC),
                              NULL,
                              BrowserDlgProc);
  if (!hwndDialog)
    return (void *) nsnull;

  // Stick a menu onto it
  if (chromeFlags & nsIWebBrowserChrome::CHROME_MENUBAR) {
    HMENU hmenuDlg = LoadMenu(ghInstanceResources, MAKEINTRESOURCE(IDC_MOZEMBED));
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
  SetWindowLong(hwndBrowser, GWL_USERDATA, (LONG)chrome);  // save the browser LONG_PTR.
  SetWindowLong(hwndBrowser, GWL_STYLE, GetWindowLong(hwndBrowser, GWL_STYLE) | WS_CLIPCHILDREN);

  // Activate the window
  PostMessage(hwndDialog, WM_ACTIVATE, WA_ACTIVE, 0);

  gDialogCount++;

  return (void *) hwndBrowser;
}
