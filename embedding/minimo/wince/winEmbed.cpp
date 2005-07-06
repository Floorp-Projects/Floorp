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

#include "MinimoPrivate.h"

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

// Global variables
static PRBool    gRunCondition = PR_TRUE;
static UINT      gBrowserCount = 0;
static UINT      gActivateCount = 0;

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


nsresult StartupProfile()
{    
	nsCOMPtr<nsIFile> appDataDir;
	nsresult rv = NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR, getter_AddRefs(appDataDir));
	if (NS_FAILED(rv))
        return rv;
    
	rv = appDataDir->Append(NS_LITERAL_STRING("minimo"));
	if (NS_FAILED(rv))
        return rv;

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

void SetPreferences()
{
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefBranch)
        return;


}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    nsIWebBrowserChrome *chrome = (nsIWebBrowserChrome *) GetWindowLong(hWnd, GWL_USERDATA);
    nsCOMPtr<nsIWebBrowser> webBrowser;
    if (chrome)
        chrome->GetWebBrowser(getter_AddRefs(webBrowser));

	switch (Msg) 
	{

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            WebBrowserChromeUI::Destroy(chrome);
            return TRUE;
        }
        break;

    case WM_ACTIVATE:
        {
            SetTimer(hWnd, 1, 500, NULL);
            nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(webBrowser));
            
            switch (wParam)
            {
            case WA_ACTIVE:
                gActivateCount++;
                if (focus)
                    focus->Activate();
                break;
                
            case WA_INACTIVE:
                gActivateCount--;
                if (focus)
                    focus->Deactivate();
            }
        }
        return 0;
        
    case WM_CLOSE:
        WebBrowserChromeUI::Destroy(chrome);
        return 0;

    case WM_DESTROY:
        if (gBrowserCount == 0)
            gRunCondition = PR_FALSE;
        return 0;

    case WM_TIMER:
        if (gActivateCount)
        {
            PRBool eventAvail;

            nsCOMPtr<nsIEventQueue> eventQ;
            nsCOMPtr<nsIEventQueueService> eqs = do_GetService(kEventQueueServiceCID);
            if (!eqs)
                return 0;

            eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));

            if (!eventQ)
                return 0;

            eventQ->PendingEvents(&eventAvail);

            if (eventAvail)
                eventQ->ProcessPendingEvents();

            // reset the timer
            SetTimer(hWnd, 1, 500, NULL);
        }
        return 0;

    case WM_SIZE:
        {
            ResizeEmbedding(chrome);
			return 0;
        }
    default:
        return DefWindowProc(hWnd, Msg, wParam, lParam);
	}


	return -1; // default handles all!
}


void RegisterMainWindowClass()
{
	WNDCLASS wc;
    
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC) MainWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(GRAY_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = "Minimo Main Window";
    
	RegisterClass(&wc);
    
}

PRUint32 RunEventLoop(PRBool &aRunCondition)
{
    MSG msg;
    while (aRunCondition ) 
    {
        ::GetMessage(&msg, NULL, 0, 0);
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
        
    return msg.wParam;
}

PRBool CheckForProcess()
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

    return TRUE;
}


int main(int argc, char *argv[])
{
    if (!CheckForProcess())
        return 0;

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
    
	RegisterMainWindowClass();

    SetPreferences();

    WindowCreator *creatorCallback = new WindowCreator();
    if (!creatorCallback)
        return 1;

    const static char* start_url = "chrome://minimo/content/minimo.xul";
    //const static char* start_url = "http://www.meer.net/~dougt/test.html";
    
    nsCOMPtr<nsIDOMWindow> newWindow;
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    
    wwatch->SetWindowCreator(NS_STATIC_CAST(nsIWindowCreator *, creatorCallback));
    wwatch->OpenWindow(nsnull, start_url, "_blank", "chrome,dialog=no,all", nsnull, getter_AddRefs(newWindow));
    
    RunEventLoop(gRunCondition);

    // Null things out before going away.
    
    newWindow = nsnull;
    wwatch = nsnull;

    // Close down Embedding APIs
    NS_TermEmbedding();

    return NS_OK;
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
                                   GetModuleHandle(NULL), 
                                   NULL);

    SetWindowLong(mainWindow, GWL_USERDATA, (LONG)chrome);  // save the browser LONG_PTR.

    PostMessage(mainWindow, WM_ACTIVATE, WA_ACTIVE, 0);

    SHFullScreen(mainWindow, SHFS_HIDETASKBAR);
    SHFullScreen(mainWindow, SHFS_HIDESIPBUTTON);

    ::ShowWindow(mainWindow, SW_SHOW);

    gBrowserCount++;

    return mainWindow;
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
    
    nsCOMPtr<nsIEmbeddingSiteWindow> baseWindow = do_QueryInterface(chrome);
    HWND hwndDlg = NULL;
    baseWindow->GetSiteWindow((void **) & hwndDlg);
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

void WebBrowserChromeUI::Destroyed(nsIWebBrowserChrome* chrome)
{
    nsCOMPtr<nsIEmbeddingSiteWindow> baseWindow = do_QueryInterface(chrome);
    HWND hwnd = NULL;
    baseWindow->GetSiteWindow((void **) & hwnd);
    if (hwnd)
        DestroyWindow(hwnd);

    if (--gBrowserCount == 0)
    {
        // Quit when there are no more browser objects
        PostQuitMessage(0);
    }
}

