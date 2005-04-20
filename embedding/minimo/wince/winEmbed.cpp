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

#include <initguid.h> // for IID_DestNetInternet

#include "MinimoPrivate.h"

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

#define STATUS_DIALOG_H 50
#define HEADSUP_DIALOG_H 300

#define MENU_HEIGHT 26

// Foward declarations of functions included in this code module:
static BOOL    CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL    CALLBACK StatusDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL    CALLBACK HeadsUpDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static nsresult OverrideMozillaComponents();
static nsresult InitializeWindowCreator();
static nsresult OpenWebPage(const char * url);
static nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome);

// Profile chooser stuff
static nsresult StartupProfile();

// Global variables
static PRBool    gRunCondition = PR_TRUE;
static UINT      gDialogCount = 0;
static HINSTANCE ghInstanceApp = NULL;
static BOOL      gActive = TRUE;
static BOOL      gBusy = TRUE;
static HANDLE    gConnectionHandle = NULL;

// minimo/wince/ list of URLs to populate the URL drop down list with
static const TCHAR *gDefaultURLs[] =
{
    _T("http://browsertest.web.aol.com/tests/javascript/javascpt/index.htm"),
    _T("http://mecha.mozilla.org/buster/test_url_25.html"),
    _T("http://www.cnn.com/"),
    _T("http://www.disney.com/"),
    _T("http://www.ebay.com/"),
    _T("http://www.go.com/"),
    _T("http://www.google.com/"),
    _T("http://www.hotmail.com/"),
    _T("http://www.javasoft.com/")
    _T("http://www.mozilla.org/"),
    _T("http://www.mozilla.org/quality/browser/standards/html/"),
    _T("http://www.netscape.com/"),
    _T("http://www.quicken.com/"),
    _T("http://www.shockwave.com/"),
    _T("http://www.slashdot.org/"),
    _T("http://www.travelocity.com/"),
    _T("http://www.yahoo.com/"),
};


typedef struct MinimoWindowContext
{
    HWND mMainWindow;
    HWND mStatusWindow;
    HWND mHeadsUpWindow;
    
} MinimoWindowContext;


PRBool LowOnMemory()
{
    MEMORYSTATUS memStats;
    memStats.dwLength = sizeof( memStats);
    
    GlobalMemoryStatus( (LPMEMORYSTATUS)&memStats );
    
    if (memStats.dwAvailPhys <= (262144) /*1024*256*/)
        return PR_TRUE;
    return PR_FALSE; 
}

void AdjustSIP(BOOL bRaise)
{
    HWND hnd;
    
    if( hnd = FindWindow(_T("SipWndClass"),NULL) )
    {
        if( bRaise ) {
            ShowWindow(hnd, SW_SHOW);
        } else {
            ShowWindow(hnd, SW_HIDE);
        }
    }
}

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

HWND GetBrowserDlgFromChrome(nsIWebBrowserChrome *aChrome)
{
    return GetParent(GetBrowserFromChrome(aChrome));
}

void DoPPCConnection(nsIWebBrowserChrome* aChrome)
{
    if (gConnectionHandle) {
        ConnMgrReleaseConnection(gConnectionHandle,0);
        gConnectionHandle = nsnull;
    }
    
    GUID gNetworkID;
    (void) ConnMgrMapURL("http://www.mozilla.org", &gNetworkID, 0);
	
	DWORD status;
	CONNMGR_CONNECTIONINFO conn_info;
	memset(&conn_info, 0, sizeof(CONNMGR_CONNECTIONINFO));
    
	conn_info.cbSize     = sizeof(CONNMGR_CONNECTIONINFO);
	conn_info.dwParams   = CONNMGR_PARAM_GUIDDESTNET;
	conn_info.dwPriority = CONNMGR_PRIORITY_USERINTERACTIVE;
	conn_info.bExclusive = FALSE;
	conn_info.bDisabled  = FALSE;
    
    conn_info.guidDestNet= gNetworkID;
    
    WebBrowserChromeUI::UpdateStatusBarText(aChrome, L"Attempting data communication");
    
    printf("Trying GUID from ConnMgrMapURL\n");
    
	if(ConnMgrEstablishConnectionSync(&conn_info, &gConnectionHandle, 1000, &status) != S_OK &&
       gNetworkID != IID_DestNetInternet)
    {
        conn_info.guidDestNet = IID_DestNetInternet;
        
        printf("Trying GUID IID_DestNetInternet\n");
        
        if(ConnMgrEstablishConnectionSync(&conn_info, &gConnectionHandle, 5000, &status) != S_OK)
        {
            WebBrowserChromeUI::UpdateStatusBarText(aChrome, L"Could not connect to network");
            return;
        }
    }
    
    if (status != CONNMGR_STATUS_CONNECTED)
        WebBrowserChromeUI::UpdateStatusBarText(aChrome, L"Could not connect to network. Status isn't connected");
}

PRBool SetupAndVerifyPPCConnection(nsIWebBrowserChrome* aChrome)
{
    DWORD status;
    if (!gConnectionHandle)
        DoPPCConnection(aChrome);
    
    if (gConnectionHandle)
    {
        ConnMgrConnectionStatus(gConnectionHandle, &status);
        if (status != CONNMGR_STATUS_CONNECTED)
        {
            DoPPCConnection(aChrome);
        }
    }
    
    if (status != CONNMGR_STATUS_CONNECTED)
        return PR_FALSE;
    
    return PR_TRUE;
}


LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) 
	{

    case WM_ACTIVATE:
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
	return 0;
}


void RegisterMainWindowClass()
{
	WNDCLASS wc;
    
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC) MainWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = ghInstanceApp;
	wc.hIcon = NULL; //LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BASICDIALOG));
	wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(GRAY_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = "Minimo Main Window";
    
	RegisterClass(&wc);
    
}

int main(int argc, char *argv[])
{
    const HANDLE hMutex = CreateMutexW(0, 0, L"_MINIMO_EXE_MUTEX_");
    
	if(0 != hMutex) 
    {
		if(ERROR_ALREADY_EXISTS == GetLastError()) 
        {
            CloseHandle(hMutex);
            
            HWND hWndExistingInstance = FindWindowW(NULL, L"Minimo");
            
            if (!hWndExistingInstance)
            {
                Sleep(1000);
                hWndExistingInstance = FindWindowW(NULL, L"Minimo");
            }
            
            if (hWndExistingInstance)
            {
                SetForegroundWindow (hWndExistingInstance);    
                return FALSE;
            }
            
            // Couldn't find the window, probably the other
            // application is starting up.  Lets just exit.
            return FALSE;
        }
    }
	else
	{
		return FALSE;
	}
    
    ghInstanceApp = GetModuleHandle(NULL);
    
#ifdef _BUILD_STATIC_BIN
    // Initialize XPCOM's module info table
    NSGetStaticModuleInfo = app_getModuleInfo;
#endif
    
    // Init Embedding APIs
    NS_InitEmbedding(nsnull, nsnull);
    
    // Choose the new profile
    if (NS_FAILED(StartupProfile()))
    {
        NS_TermEmbedding();
        return 1;
    }
    
    InitializeWindowCreator();
    
    OverrideMozillaComponents();
    
	RegisterMainWindowClass();
    
	WSADATA wsaData;
	WSAStartup( MAKEWORD( 1, 1 ), &wsaData );
    
    // Open the initial browser window
    OpenWebPage("resource://gre/res/start.html");
    
    WPARAM rv = AppCallbacks::RunEventLoop(gRunCondition);
    
    // Close down Embedding APIs
    NS_TermEmbedding();
    
    
    if (gConnectionHandle)
        ConnMgrReleaseConnection(gConnectionHandle,0);
    
	WSACleanup();
    
    return rv;
}


nsresult OverrideMozillaComponents()
{
    // Register our own native prompting service for message boxes, login
    // prompts etc.
    nsCOMPtr<nsIFactory> promptFactory;
    nsresult rv = NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIComponentRegistrar> registrar;
    rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (NS_FAILED(rv)) return rv;
    
    static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);
    
    rv = registrar->RegisterFactory(kPromptServiceCID,
                                    "Prompt Service",
                                    NS_PROMPTSERVICE_CONTRACTID,
                                    promptFactory);
    return rv;
}

/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This is how all new windows are opened,
   except any created directly by the embedding app. */
nsresult InitializeWindowCreator()
{
    // create an nsWindowCreator and give it to the WindowWatcher service
    WindowCreator *creatorCallback = new WindowCreator();
    if (creatorCallback)
    {
        nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator *, creatorCallback));
        if (windowCreator)
        {
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
            if (wwatch)
            {
                wwatch->SetWindowCreator(windowCreator);
                return NS_OK;
            }
        }
    }
    return NS_ERROR_FAILURE;
}

void LoadURL(nsIWebBrowserChrome* aChrome, nsIWebNavigation* webNavigation, char* url)
{
    WebBrowserChromeUI::UpdateBusyState(aChrome, PR_TRUE);
    
    SetupAndVerifyPPCConnection(aChrome);
    
    webNavigation->LoadURI(NS_ConvertASCIItoUCS2(url).get(),
                           nsIWebNavigation::LOAD_FLAGS_NONE,
                           nsnull,
                           nsnull,
                           nsnull);
}

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
        
        LoadURL(chrome, webNav, (char*)url);
        return NS_OK;
    }
    
    return rv;
}

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
    
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(webBrowser);
    if (clipCmds)
    {
        clipCmds->CanCutSelection(&canCutSelection);
        clipCmds->CanCopySelection(&canCopySelection);
        clipCmds->CanPaste(&canPaste);
    }
    
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
    if (!mwcontext)
		return;
    
    HWND button;
    
    button = GetDlgItem(mwcontext->mStatusWindow, IDC_BACK);
    if (button)
        EnableWindow(button, canGoBack);
    
    button = GetDlgItem(mwcontext->mStatusWindow, IDC_FORWARD);
    if (button)
        EnableWindow(button, canGoForward);
}

void LoadURLInURLBar(HWND hwndDlg, nsIWebBrowserChrome* chrome, nsIWebNavigation* webNavigation)
{
    TCHAR szURL[2048];
    memset(szURL, 0, sizeof(szURL));
    GetDlgItemText(hwndDlg, IDC_ADDRESS, szURL, sizeof(szURL) / sizeof(szURL[0]) - 1);
    
    LoadURL(chrome, webNavigation, szURL);
}

BOOL CALLBACK HeadsUpDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!mwcontext)
		return FALSE;
    
    HWND hwndBrowser = GetDlgItem(mwcontext->mMainWindow, IDC_BROWSER);
    
    nsIWebBrowserChrome *chrome = nsnull ;
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
    
    switch (uMsg)
    {
    case WM_COMMAND:
        {
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
                else if (HIWORD(wParam) == CBN_SELENDOK)
                {
                    HWND hwndAddress = GetDlgItem(hwndDlg, IDC_ADDRESS);
                    
                    // Find the current selected item and get the item's length.
                    int t = SendMessage(hwndAddress, CB_GETCURSEL, 0, 0);
                    int l = SendMessage(hwndAddress, CB_GETLBTEXTLEN, t, 0);
                    
                    // Get the text
                    char* buffer = new char[l + 1];
                    SendMessage(hwndAddress, CB_GETLBTEXT, t, (long) buffer);
                    
                    // Set the text area so that we can see it
                    SetDlgItemText(hwndDlg, IDC_ADDRESS, buffer);
                    
                    delete buffer;
                }
                break;
                
            case IDC_GO:
                LoadURLInURLBar(hwndDlg, chrome, webNavigation);
                ::ShowWindow(hwndDlg, SW_HIDE);
                break;
                
            case IDC_STOP:
                webNavigation->Stop(nsIWebNavigation::STOP_ALL);
                UpdateUI(chrome);
                break;
                
            case IDC_RELOAD:
                webNavigation->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
                break;
                
            case IDC_Hide:
                {
                    RECT rect;
                    ::ShowWindow(mwcontext->mHeadsUpWindow, SW_HIDE);
                    ::GetWindowRect(mwcontext->mHeadsUpWindow, &rect);
                    ::InvalidateRect(mwcontext->mMainWindow, &rect, TRUE);
                    ::SetForegroundWindow(mwcontext->mMainWindow);
                    break;
                }
                
            case IDM_EXIT:
                PostMessage(hwndDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
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
                
            }
        }
    }
    return FALSE;
}


static HBRUSH yellowBrush = NULL;

BOOL CALLBACK StatusDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!mwcontext)
	    return FALSE;
    
    HWND hwndBrowser = GetDlgItem(mwcontext->mMainWindow, IDC_BROWSER);
    
    nsIWebBrowserChrome *chrome = nsnull ;
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
    
    switch (uMsg) {
        
    case WM_INITDIALOG:
        {
            yellowBrush = CreateSolidBrush(RGB(255,255,204));
        }
        return TRUE;
        
    case WM_CTLCOLORDLG:
        return (BOOL) yellowBrush;
        
    case WM_DESTROY:
        DeleteObject(yellowBrush);
        break;
        
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_STOP:
                webNavigation->Stop(nsIWebNavigation::STOP_ALL);
                UpdateUI(chrome);
            }
        }
    }
    return FALSE;
}

BOOL CALLBACK BrowserDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Get the browser and other pointers since they are used a lot below
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    nsIWebBrowserChrome *chrome = nsnull ;
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
    
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);        
	if (!mwcontext)
	    return FALSE;
    
    // Test the message
    switch (uMsg)
    {
    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            WebBrowserChromeUI::Destroy(chrome);
            return TRUE;
        }
        break;
        
    case WM_HIBERNATE: /* not sure about this */
        // fall through.
    case WM_CLOSE:
        {
            gActive = FALSE;
            gRunCondition = FALSE;
            
            DestroyWindow(mwcontext->mStatusWindow);
            DestroyWindow(mwcontext->mHeadsUpWindow);
            DestroyWindow(mwcontext->mMainWindow);
            free((void*) mwcontext);
        }
        return 0;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
        
    case WM_ACTIVATE:
        {
            nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(webBrowser));
            if (!focus)
                return 0;

            switch (wParam)
            {
            case WA_ACTIVE:
                gActive = TRUE;
                if (focus)
                    focus->Activate();
                break;
                
            case WA_INACTIVE:
                if (focus)
                    focus->Deactivate();
            default:
                break;
            }
        }
        return 0;
        
    case WM_SIZE:
        {
            if (!mwcontext || !chrome)
                return 0;
        
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            int screenW = GetSystemMetrics(SM_CXSCREEN) ;
            
            MoveWindow(mwcontext->mMainWindow,    0, MENU_HEIGHT, screenW, screenH - MENU_HEIGHT, TRUE);
            
            RECT rt;
            GetClientRect(mwcontext->mMainWindow, &rt);
            
            MoveWindow(mwcontext->mHeadsUpWindow, rt.left, rt.top, rt.right, HEADSUP_DIALOG_H, TRUE);
            MoveWindow(mwcontext->mStatusWindow,  rt.left, rt.top, rt.right, STATUS_DIALOG_H, TRUE);
            
            HWND hwndBrowser = GetDlgItem(mwcontext->mMainWindow, IDC_BROWSER);
            if (hwndBrowser)
                MoveWindow(hwndBrowser, rt.left, rt.top, rt.right, rt.bottom, TRUE);
            
            ResizeEmbedding(chrome);
            return 0;
        }
    }
    return FALSE;
}

nsresult StartupProfile()
{    
	nsCOMPtr<nsIFile> appDataDir;
	nsresult rv = NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR, getter_AddRefs(appDataDir));
	if (NS_FAILED(rv))
        return rv;
    
	appDataDir->Append(NS_LITERAL_STRING("winembed"));
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

void
KillCurrentLoad(nsIWebBrowserChrome* aChrome)
{
    nsCOMPtr<nsIWebBrowser> webBrowser;
    nsCOMPtr<nsIWebNavigation> webNavigation;
    
    aChrome->GetWebBrowser(getter_AddRefs(webBrowser));
    webNavigation = do_QueryInterface(webBrowser);
    if (webNavigation)
        webNavigation->Stop(nsIWebNavigation::STOP_ALL);
    
    HWND win = GetBrowserDlgFromChrome(aChrome);
    
    WebBrowserChromeUI::UpdateStatusBarText(aChrome, L"Running Low on Memory. Cancelling page load.");
    
    MessageBox(win, "Running Low on Memory. Cancelling page load.", "Note", 0);
    
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(win, GWL_USERDATA);
    if (!mwcontext)
	    return;
    
    ::ShowWindow(mwcontext->mStatusWindow, SW_HIDE);
}

nativeWindow 
WebBrowserChromeUI::CreateNativeWindow(nsIWebBrowserChrome* chrome)
{
    
	HWND mainWindow = CreateWindow("Minimo Main Window", 
                                   "Minimo", 
                                   WS_VISIBLE, 
                                   CW_USEDEFAULT, 
                                   CW_USEDEFAULT, 
                                   CW_USEDEFAULT, 
                                   CW_USEDEFAULT, 
                                   NULL, 
                                   NULL, 
                                   ghInstanceApp, 
                                   NULL);
    
    
    
    // Load the browser dialog from resource
    HWND hwndDialog = CreateDialog(ghInstanceApp,
                                   MAKEINTRESOURCE(IDD_BROWSER),
                                   mainWindow,
                                   BrowserDlgProc);
    if (!hwndDialog)
        return NULL;
    
    HWND statusDialog = CreateDialog(ghInstanceApp,
                                     MAKEINTRESOURCE(IDD_STATUS_DISPLAY),
                                     mainWindow,
                                     StatusDlgProc);
    
    if (!statusDialog) {
        MessageBox(0, "statusDialog failed", "error", 0);
        return NULL;
    }
    
    HWND headsupDialog = CreateDialog(ghInstanceApp,
                                      MAKEINTRESOURCE(IDD_HEADSUP_DISPLAY),
                                      mainWindow,
                                      HeadsUpDlgProc);
    
    if (!headsupDialog)
    {
        MessageBox(0, "headsupDialog failed", "error", 0);
        return NULL;
    }
    
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int screenW = GetSystemMetrics(SM_CXSCREEN) ;
    
    MoveWindow(mainWindow,    0, MENU_HEIGHT, screenW, screenH - MENU_HEIGHT, TRUE);
    
    RECT rt;
    GetClientRect(mainWindow, &rt);
    
    MoveWindow(hwndDialog,    rt.left, rt.top, rt.right, rt.bottom, TRUE);
    MoveWindow(headsupDialog, rt.left, rt.top, rt.right, HEADSUP_DIALOG_H, TRUE);
    MoveWindow(statusDialog,  rt.left, rt.top, rt.right, STATUS_DIALOG_H, TRUE);
    
    // Fetch the browser window handle
    HWND hwndBrowser = GetDlgItem(hwndDialog, IDC_BROWSER);
    MoveWindow(hwndBrowser, rt.left, rt.top, rt.right, rt.bottom, TRUE);

    // Add some interesting URLs to the address drop down
    HWND hwndAddress = GetDlgItem(headsupDialog, IDC_ADDRESS);
    if (hwndAddress) {
        for (int i = 0; i < sizeof(gDefaultURLs) / sizeof(gDefaultURLs[0]); i++)
        {
            SendMessage(hwndAddress, CB_ADDSTRING, 0, (LPARAM) gDefaultURLs[i]);
        }
    }    
    
    SetWindowLong(hwndBrowser, GWL_USERDATA, (LONG)chrome);  // save the browser LONG_PTR.
    SetWindowLong(hwndBrowser, GWL_STYLE, GetWindowLong(hwndBrowser, GWL_STYLE) | WS_CLIPCHILDREN);
    
    MinimoWindowContext *mwcontext = (MinimoWindowContext*) malloc(sizeof(MinimoWindowContext));
    if (!mwcontext)
        return NULL;
    
    mwcontext->mMainWindow = hwndDialog;
    mwcontext->mStatusWindow = statusDialog;
    mwcontext->mHeadsUpWindow = headsupDialog;
    
    SetWindowLong(hwndDialog,    GWL_USERDATA, (LONG)mwcontext);  
    SetWindowLong(headsupDialog, GWL_USERDATA, (LONG)mwcontext);  
    SetWindowLong(statusDialog,  GWL_USERDATA, (LONG)mwcontext);  
    
    // Activate the window
    PostMessage(hwndDialog, WM_ACTIVATE, WA_ACTIVE, 0);
    
    ::ShowWindow(hwndDialog, SW_SHOW);
    
    gDialogCount++;
    
    return hwndBrowser;
}

void 
WebBrowserChromeUI::Destroy(nsIWebBrowserChrome* chrome)
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
    SetWindowLong(hwndBrowser, GWL_USERDATA, nsnull);
    DestroyWindow(hwndBrowser);
    DestroyWindow(hwndDlg);
    
    --gDialogCount;
    if (gDialogCount == 0)
    {
        // Quit when there are no more browser objects
        PostQuitMessage(0);
    }
}

void 
WebBrowserChromeUI::SetFocus(nsIWebBrowserChrome *chrome)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(chrome);
    if (hwndDlg == NULL)
    {
        return;
    }
    
    HWND hwndBrowser = GetDlgItem(hwndDlg, IDC_BROWSER);
    ::SetFocus(hwndBrowser);
}

void 
WebBrowserChromeUI::UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText)
{
    if (!aChrome)
        return;
    
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!mwcontext)
	    return;
    
    nsCString status; 
    if (aStatusText)
        status.AssignWithConversion(aStatusText);
    
    SetDlgItemText(mwcontext->mStatusWindow, IDC_STATUS, status.get());
    
    if (LowOnMemory())
        KillCurrentLoad(aChrome);
}

void 
WebBrowserChromeUI::UpdateCurrentURI(nsIWebBrowserChrome *aChrome)
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
        MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
        if (mwcontext)
            SetDlgItemText(mwcontext->mHeadsUpWindow, IDC_ADDRESS, uriString.get());
    }
}

void 
WebBrowserChromeUI::UpdateBusyState(nsIWebBrowserChrome *aChrome, PRBool aBusy)
{
    if (LowOnMemory())
        KillCurrentLoad(aChrome);
    
    gBusy = aBusy;
    
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!mwcontext)
		return;
    
    if (aBusy) {
        ::ShowWindow(mwcontext->mStatusWindow, SW_SHOW);
        ::SetForegroundWindow(mwcontext->mStatusWindow);
    }
    else
    {
        RECT rect;
        ::ShowWindow(mwcontext->mStatusWindow, SW_HIDE);
        ::GetWindowRect(mwcontext->mStatusWindow, &rect);
        ::InvalidateRect(mwcontext->mMainWindow, &rect, TRUE);
        ::SetForegroundWindow(mwcontext->mMainWindow);
        
        nsMemory::HeapMinimize(PR_TRUE);
    }
    
    
    HWND button;
    button = GetDlgItem(mwcontext->mHeadsUpWindow, IDC_STOP);
    
    if (button)
        EnableWindow(button, aBusy);
    
    button = GetDlgItem(mwcontext->mHeadsUpWindow, IDC_GO);
    
    if (button)
        EnableWindow(button, !aBusy);
    
    UpdateUI(aChrome);
}

void
WebBrowserChromeUI::UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!mwcontext)
		return;
    
    HWND hwndProgress = GetDlgItem(mwcontext->mStatusWindow, IDC_PROGRESS);
    
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
    
    if (LowOnMemory())
        KillCurrentLoad(aChrome);
}

void 
WebBrowserChromeUI::ShowContextMenu(nsIWebBrowserChrome *aChrome, PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
}

void 
WebBrowserChromeUI::ShowTooltip(nsIWebBrowserChrome *aChrome, PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
}

void 
WebBrowserChromeUI::HideTooltip(nsIWebBrowserChrome *aChrome)
{
}

void 
WebBrowserChromeUI::ShowWindow(nsIWebBrowserChrome *aChrome, PRBool aShow)
{
    HWND win = GetBrowserDlgFromChrome(aChrome);
    ::ShowWindow(win, aShow ? SW_SHOW : SW_HIDE);
}

void WebBrowserChromeUI::SizeTo(nsIWebBrowserChrome *aChrome, PRInt32 aWidth, PRInt32 aHeight)
{
}

boolean 
WebBrowserChromeUI::DoubleClick(nsIWebBrowserChrome *aChrome, PRInt32 x, PRInt32 y)
{
    HWND hwndDlg = GetBrowserDlgFromChrome(aChrome);
    MinimoWindowContext* mwcontext = (MinimoWindowContext*) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!mwcontext)
		return false;
    
    HWND hwndBrowser = GetDlgItem(mwcontext->mMainWindow, IDC_BROWSER);
    
    nsIWebBrowserChrome *chrome = nsnull ;
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
    
    // Stop any current load.
    webNavigation->Stop(nsIWebNavigation::STOP_ALL);
    
    
    // Show our heads up display.
    ::ShowWindow(mwcontext->mHeadsUpWindow, SW_SHOW);
    return true;
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
    CallQueryInterface(NS_STATIC_CAST(nsIWebBrowserChrome*, chrome), aNewWindow);
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
    ResizeEmbedding(NS_STATIC_CAST(nsIWebBrowserChrome*, chrome));
    
    // if opened as chrome, it'll be made visible after the chrome has loaded.
    // otherwise, go ahead and show it now.
    if (!(aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME))
        WebBrowserChromeUI::ShowWindow(*aNewWindow, PR_TRUE);
    
    return NS_OK;
}

void AppCallbacks::EnableChromeWindow(nsIWebBrowserChrome *aWindow,
                                      PRBool aEnabled)
{
    HWND hwnd = GetBrowserDlgFromChrome(aWindow);
    ::EnableWindow(hwnd, aEnabled ? TRUE : FALSE);
}

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

PRUint32 AppCallbacks::RunEventLoop(PRBool &aRunCondition)
{
    MSG msg;
    HANDLE hFakeEvent = ::CreateEventA(NULL, TRUE, FALSE, NULL);
    
    nsCOMPtr<nsIEventQueue> eventQ;
    nsCOMPtr<nsIEventQueueService> eqs = do_GetService(kEventQueueServiceCID);
    if (!eqs)
        return -1;
    
    eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    
    while (aRunCondition ) 
    {
        // Process pending messages
        while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) 
        {
            if (!::GetMessage(&msg, NULL, 0, 0)) 
            {
                aRunCondition = PR_FALSE;
                break;
            }
            
            eventQ->ProcessPendingEvents();
            
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        
        // Do idle stuff
        if (gActive)
        {
            if (!gBusy)
                ::MsgWaitForMultipleObjects(1, &hFakeEvent, FALSE, 5000, QS_ALLEVENTS);
            
            eventQ->ProcessPendingEvents();
        }
    }
    ::CloseHandle(hFakeEvent);
    return msg.wParam;
}
