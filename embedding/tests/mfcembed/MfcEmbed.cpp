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
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// File Overview....
//
// The typical MFC app, frame creation code + AboutDlg handling
//
// NS_InitEmbedding() is called in InitInstance()
// 
// NS_TermEmbedding() is called in ExitInstance()
// ExitInstance() also takes care of cleaning up of
// multiple browser frame windows on app exit
//
// Code to handle the creation of a new browser window

// Next suggested file to look at : BrowserFrm.cpp

// Local Includes
#include "stdafx.h"
#include "MfcEmbed.h"
#include "nsXPCOMGlue.h"
#include "nsIComponentRegistrar.h"
#include "BrowserFrm.h"
#include "EditorFrm.h"
#include "winEmbedFileLocProvider.h"
#include "BrowserImpl.h"
#include "nsIWindowWatcher.h"
#include "plstr.h"
#include "Preferences.h"
#include <io.h>
#include <fcntl.h>

#ifdef USE_PROFILES
#include "ProfileMgr.h"
#else
#include "nsProfileDirServiceProvider.h"
#endif

#ifdef MOZ_PROFILESHARING
#include "nsIProfileSharingSetup.h"
#endif

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif


#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// this is for overriding the Mozilla default PromptService component
#include "PromptService.h"
#define kComponentsLibname _T("mfcEmbedComponents.dll")
#define NS_PROMPTSERVICE_CID \
 {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);

// this is for overriding the Mozilla default PrintingPromptService component
#include "PrintingPromptService.h"
#define NS_PRINTINGPROMPTSERVICE_CID \
 {0xe042570c, 0x62de, 0x4bb6, { 0xa6, 0xe0, 0x79, 0x8e, 0x3c, 0x7, 0xb4, 0xdf}}
static NS_DEFINE_CID(kPrintingPromptServiceCID, NS_PRINTINGPROMPTSERVICE_CID);

// this is for overriding the Mozilla default HelperAppLauncherDialog
#include "HelperAppService.h"
#define NS_HELPERAPPLAUNCHERDIALOG_CID \
    {0xf68578eb, 0x6ec2, 0x4169, {0xae, 0x19, 0x8c, 0x62, 0x43, 0xf0, 0xab, 0xe1}}
static NS_DEFINE_CID(kHelperAppLauncherDialogCID, NS_HELPERAPPLAUNCHERDIALOG_CID);

class CMfcEmbedCommandLine : public CCommandLineInfo
{
public:

    CMfcEmbedCommandLine(CMfcEmbedApp& app) : CCommandLineInfo(),
                                              mApp(app)
    {
    }

    // generic parser which bundles up flags and their parameters, to
    // pass to HandleFlag() or HandleNakedParameter()
    // if you're adding new parameters, please don't touch this
    // function and instead add your own handler below
    virtual void ParseParam(LPCTSTR szParam, BOOL bFlag, BOOL bLast)
    {
        CCommandLineInfo::ParseParam(szParam, bFlag, bLast);
        if (bFlag) {
            // advance past extra stuff like --foo
            while (*szParam && *szParam == '-')
                szParam++;

            // previous argument was a flag too, so process that first
            if (!mLastFlag.IsEmpty())
                HandleFlag(mLastFlag);
            
            mLastFlag = szParam;

            // oops, no more arguments coming, so handle this now
            if (bLast)
                HandleFlag(mLastFlag);
            
        } else {
            if (!mLastFlag.IsEmpty())
                HandleFlag(mLastFlag, szParam);
                
            mLastFlag.Truncate();
        }
    }

    // handle flag-based parameters
#ifdef _UNICODE
    void HandleFlag(const nsAString& flag, const TCHAR * param = nsnull)
#else
    void HandleFlag(const nsACString& flag, const TCHAR * param = nsnull)
#endif
    {
        if (flag.Equals(_T("console")))
            DoConsole();
        else if (flag.Equals(_T("chrome")))
            DoChrome();
#ifdef NS_TRACE_MALLOC
        else if (flag.Equals(_T("trace-malloc")))
        {
            USES_CONVERSION;
            DoTraceMalloc(flag, T2CA(param));
        }
#endif
        // add new flag handlers here (please add a DoFoo() method below!)
    }

    void HandleNakedParameter(const char* flag) {
        // handle non-flag arguments here
    }

    // add your specific handlers here
    void DoConsole() {
        mApp.ShowDebugConsole();
    }

    void DoChrome() {
        mApp.m_bChrome = TRUE;
    }

#ifdef NS_TRACE_MALLOC
    void DoTraceMalloc(const nsACString& flag, const char* param)
    {
        if (!param) {
            NS_WARNING("--trace-malloc needs a filename as a parameter");
            return;
        }

        // build up fake argv/argc arguments for tracemalloc stuff
        char* argv[] = { "mfcembed", "--trace-malloc",
                         NS_CONST_CAST(char*, param) };
        
        NS_TraceMallocStartupArgs(3, argv);
    }
#endif
    
private:
    // autostring is fine, this is a stack based object anyway
#ifdef _UNICODE
    nsAutoString mLastFlag;
#else
    nsCAutoString mLastFlag;
#endif

    CMfcEmbedApp& mApp;
};


BEGIN_MESSAGE_MAP(CMfcEmbedApp, CWinApp)
    //{{AFX_MSG_MAP(CMfcEmbedApp)
    ON_COMMAND(ID_NEW_BROWSER, OnNewBrowser)
    ON_COMMAND(ID_NEW_EDITORWINDOW, OnNewEditor)
#ifdef USE_PROFILES
    ON_COMMAND(ID_MANAGE_PROFILES, OnManageProfiles)
#endif
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_EDIT_PREFERENCES, OnEditPreferences)
    // NOTE - the ClassWizard will add and remove mapping macros here.
    //    DO NOT EDIT what you see in these blocks of generated code!
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMfcEmbedApp::CMfcEmbedApp()
{
    mRefCnt = 1; // Start at one - nothing is going to addref this object

#ifdef USE_PROFILES
    m_ProfileMgr = NULL;
#endif

    m_strHomePage = "";

    m_iStartupPage = 0; 

    m_bChrome = FALSE;
}

CMfcEmbedApp theApp;

/* Some Gecko interfaces are implemented as components, automatically
   registered at application initialization. nsIPrompt is an example:
   the default implementation uses XUL, not native windows. Embedding
   apps can override the default implementation by implementing the
   nsIPromptService interface and registering a factory for it with
   the same CID and Contract ID as the default's.

   Note that this example implements the service in a separate DLL,
   replacing the default if the override DLL is present. This could
   also have been done in the same module, without a separate DLL.
   See the PowerPlant example for, well, an example.
*/
nsresult CMfcEmbedApp::OverrideComponents()
{
    nsCOMPtr<nsIComponentRegistrar> compReg;
    nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(compReg));
    NS_ENSURE_SUCCESS(rv, rv);
    
    // replace Mozilla's default PromptService with our own, if the
    // expected override DLL is present
    HMODULE overlib = ::LoadLibrary(kComponentsLibname);
    if (overlib) {
        InitPromptServiceType InitLib;
        MakeFactoryType MakeFactory;
        InitLib = reinterpret_cast<InitPromptServiceType>(::GetProcAddress(overlib, kPromptServiceInitFuncName));
        MakeFactory = reinterpret_cast<MakeFactoryType>(::GetProcAddress(overlib, kPromptServiceFactoryFuncName));

        if (InitLib && MakeFactory) {
            InitLib(overlib);

            nsCOMPtr<nsIFactory> promptFactory;
            rv = MakeFactory(getter_AddRefs(promptFactory));
            if (NS_SUCCEEDED(rv)) {
                compReg->RegisterFactory(kPromptServiceCID,
                                         "Prompt Service",
                                         "@mozilla.org/embedcomp/prompt-service;1",
                                         promptFactory);
            }
        } else
          ::FreeLibrary(overlib);
    }

    // Replace Mozilla's helper app launcher dialog with our own
    overlib = ::LoadLibrary(kComponentsLibname);
    if (overlib) {
        InitHelperAppDlgType InitLib;
        MakeFactoryType MakeFactory;
        InitLib = reinterpret_cast<InitHelperAppDlgType>(::GetProcAddress(overlib, kHelperAppDlgInitFuncName));
        MakeFactory = reinterpret_cast<MakeFactoryType>(::GetProcAddress(overlib, kHelperAppDlgFactoryFuncName));

        if (InitLib && MakeFactory) {
            InitLib(overlib);

            nsCOMPtr<nsIFactory> helperAppDlgFactory;
            rv = MakeFactory(getter_AddRefs(helperAppDlgFactory));
            if (NS_SUCCEEDED(rv))
                compReg->RegisterFactory(kHelperAppLauncherDialogCID,
                                         "Helper App Launcher Dialog",
                                         "@mozilla.org/helperapplauncherdialog;1",
                                         helperAppDlgFactory);
        } else
          ::FreeLibrary(overlib);
    }

    // replace Mozilla's default PrintingPromptService with our own, if the
    // expected override DLL is present
    overlib = ::LoadLibrary(kComponentsLibname);
    if (overlib) {
        InitPrintingPromptServiceType InitLib;
        MakeFactoryType MakeFactory;
        InitLib = reinterpret_cast<InitPrintingPromptServiceType>(::GetProcAddress(overlib, kPrintingPromptServiceInitFuncName));
        MakeFactory = reinterpret_cast<MakeFactoryType>(::GetProcAddress(overlib, kPrintingPromptServiceFactoryFuncName));

        if (InitLib && MakeFactory) {
            InitLib(overlib);

            nsCOMPtr<nsIFactory> printingPromptFactory;
            rv = MakeFactory(getter_AddRefs(printingPromptFactory));
            if (NS_SUCCEEDED(rv))
                compReg->RegisterFactory(kPrintingPromptServiceCID,
                                         "Printing Prompt Service",
                                         "@mozilla.org/embedcomp/printingprompt-service;1",
                                         printingPromptFactory);
        } else
          ::FreeLibrary(overlib);
    }
    return rv;
}

void CMfcEmbedApp::ShowDebugConsole()
{
#ifdef _DEBUG
    // Show console only in debug mode

    if(! AllocConsole())
        return;

    // Redirect stdout to the console
    int hCrtOut = _open_osfhandle(
                (long) GetStdHandle(STD_OUTPUT_HANDLE),
                _O_TEXT);
    if(hCrtOut == -1)
        return;

    FILE *hfOut = _fdopen(hCrtOut, "w");
    if(hfOut != NULL)
    {
        // Setup for unbuffered I/O so the console 
        // output shows up right away
        *stdout = *hfOut;
        setvbuf(stdout, NULL, _IONBF, 0); 
    }

    // Redirect stderr to the console
    int hCrtErr = _open_osfhandle(
                (long) GetStdHandle(STD_ERROR_HANDLE),
                _O_TEXT);
    if(hCrtErr == -1)
        return;

    FILE *hfErr = _fdopen(hCrtErr, "w");
    if(hfErr != NULL)
    {
        // Setup for unbuffered I/O so the console 
        // output shows up right away
        *stderr = *hfErr;
        setvbuf(stderr, NULL, _IONBF, 0); 
    }
#endif
}


// Initialize our MFC application and also init
// the Gecko embedding APIs
// Note that we're also init'ng the profile switching
// code here
// Then, create a new BrowserFrame and load our
// default homepage
//
BOOL CMfcEmbedApp::InitInstance()
{
#ifdef _BUILD_STATIC_BIN
    // Initialize XPCOM's module info table
    NSGetStaticModuleInfo = app_getModuleInfo;
#endif

    CMfcEmbedCommandLine cmdLine(*this);
    ParseCommandLine(cmdLine);
    
    Enable3dControls();

#ifdef XPCOM_GLUE
    if (NS_FAILED(XPCOMGlueStartup(GRE_GetXPCOMPath()))) {
        MessageBox(NULL, "Could not initialize XPCOM. Perhaps the GRE\nis not installed or could not be found?", "MFCEmbed", MB_OK | MB_ICONERROR);
        return FALSE;
    }
#endif

    //
    // 1. Determine the name of the dir from which the GRE based app is being run
    // from [It's OK to do this even if you're not running in an GRE env]
    //
    // 2. Create an nsILocalFile out of it which will passed in to NS_InitEmbedding()
    //
    // Please see http://www.mozilla.org/projects/embedding/GRE.html
    // for more info. on GRE

    TCHAR path[_MAX_PATH+1];
    ::GetModuleFileName(0, path, _MAX_PATH);
    TCHAR* lastSlash = _tcsrchr(path, _T('\\'));
    if (!lastSlash) {
        NS_ERROR("No slash in module file name... something is wrong.");
        return FALSE;
    }
    *lastSlash = _T('\0');

    USES_CONVERSION;
    nsresult rv;
    nsCOMPtr<nsILocalFile> mreAppDir;
    rv = NS_NewNativeLocalFile(nsDependentCString(T2A(path)), TRUE, getter_AddRefs(mreAppDir));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create mreAppDir localfile");

    // Take a look at 
    // http://www.mozilla.org/projects/xpcom/file_locations.html
    // for more info on File Locations

    CString strRes;
    strRes.LoadString(IDS_PROFILES_FOLDER_NAME);
    winEmbedFileLocProvider *provider = new winEmbedFileLocProvider(nsDependentCString(strRes));
    if(!provider)
    {
        ASSERT(FALSE);
        return FALSE;
    }

    rv = NS_InitEmbedding(mreAppDir, provider);
    if(NS_FAILED(rv))
    {
        ASSERT(FALSE);
        return FALSE;
    }

    rv = OverrideComponents();
    if(NS_FAILED(rv))
    {
        ASSERT(FALSE);
        return FALSE;
    }


    rv = InitializeWindowCreator();
    if (NS_FAILED(rv))
    {
        ASSERT(FALSE);
        return FALSE;
    }

    if(!InitializeProfiles())
    {
        ASSERT(FALSE);
        NS_TermEmbedding();
        return FALSE;
    }


    if(!CreateHiddenWindow())
    {
        ASSERT(FALSE);
        NS_TermEmbedding();
        return FALSE;
    }

    // Create the first browser frame window
    OnNewBrowser();

    return TRUE;
}

CBrowserFrame* CMfcEmbedApp::CreateNewBrowserFrame(PRUint32 chromeMask,
                                                   PRInt32 x, PRInt32 y,
                                                   PRInt32 cx, PRInt32 cy,
                                                   PRBool bShowWindow,
                                                   PRBool bIsEditor
                                                   )
{
    UINT resId = bIsEditor ? IDR_EDITOR : IDR_MAINFRAME;

    // Setup a CRect with the requested window dimensions
    CRect winSize(x, y, cx, cy);

    // Use the Windows default if all are specified as -1
    if(x == -1 && y == -1 && cx == -1 && cy == -1)
        winSize = CFrameWnd::rectDefault;

    // Load the window title from the string resource table
    CString strTitle;
    strTitle.LoadString(IDR_MAINFRAME);

    // Now, create the browser frame
    CBrowserFrame* pFrame = bIsEditor ? ( new  CEditorFrame(chromeMask) ) :
                        ( new  CBrowserFrame(chromeMask) );
    pFrame->SetEditable(bIsEditor);

    if (!pFrame->Create(NULL, strTitle, WS_OVERLAPPEDWINDOW, 
                    winSize, NULL, MAKEINTRESOURCE(resId), 0L, NULL))
    {
        return NULL;
    }

    // load accelerator resource
    pFrame->LoadAccelTable(MAKEINTRESOURCE(IDR_MAINFRAME));

    // Show the window...
    if(bShowWindow)
    {
        pFrame->ShowWindow(SW_SHOW);
        pFrame->UpdateWindow();
    }

    // Add to the list of BrowserFrame windows
    m_FrameWndLst.AddHead(pFrame);

    return pFrame;
}

void CMfcEmbedApp::OnNewBrowser()
{
    CBrowserFrame *pBrowserFrame = CreateNewBrowserFrame();

    //Load the HomePage into the browser view
    if(pBrowserFrame && (GetStartupPageMode() == 1))
        pBrowserFrame->m_wndBrowserView.LoadHomePage();
}

void CMfcEmbedApp::OnNewEditor() 
{
    CEditorFrame *pEditorFrame = (CEditorFrame *)CreateNewBrowserFrame(nsIWebBrowserChrome::CHROME_ALL, 
                                    -1, -1, -1, -1,
                                    PR_TRUE,PR_TRUE);
    if (pEditorFrame)
    {
        pEditorFrame->InitEditor();
        pEditorFrame->m_wndBrowserView.OpenURL("about:blank");
    }
}

// This gets called anytime a BrowserFrameWindow is
// closed i.e. by choosing the "close" menu item from
// a window's system menu or by dbl clicking on the
// system menu box
// 
// Sends a WM_QUIT to the hidden window which results
// in ExitInstance() being called and the app is
// properly cleaned up and shutdown
//
void CMfcEmbedApp::RemoveFrameFromList(CBrowserFrame* pFrm, BOOL bCloseAppOnLastFrame/*= TRUE*/)
{
    POSITION pos = m_FrameWndLst.Find(pFrm);
    m_FrameWndLst.RemoveAt(pos);

    // Send a WM_QUIT msg. to the hidden window if we've
    // just closed the last browserframe window and
    // if the bCloseAppOnLastFrame is TRUE. This be FALSE
    // only in the case we're switching profiles
    // Without this the hidden window will stick around
    // i.e. the app will never die even after all the 
    // visible windows are gone.
    if(m_FrameWndLst.GetCount() == 0 && bCloseAppOnLastFrame)
        m_pMainWnd->PostMessage(WM_QUIT);
}

int CMfcEmbedApp::ExitInstance()
{
    // When File/Exit is chosen and if the user
    // has opened multiple browser windows shut all
    // of them down properly before exiting the app

    CBrowserFrame* pBrowserFrame = NULL;

    POSITION pos = m_FrameWndLst.GetHeadPosition();
    while( pos != NULL )
    {
        pBrowserFrame = (CBrowserFrame *) m_FrameWndLst.GetNext(pos);
        if(pBrowserFrame)
        {
            pBrowserFrame->ShowWindow(false);
            pBrowserFrame->DestroyWindow();
        }
    }
    m_FrameWndLst.RemoveAll();

    if (m_pMainWnd)
        m_pMainWnd->DestroyWindow();

#ifdef USE_PROFILES
    delete m_ProfileMgr;
#else
    if (m_ProfileDirServiceProvider)
    {
        m_ProfileDirServiceProvider->Shutdown();
        NS_RELEASE(m_ProfileDirServiceProvider);
    }
#endif

    NS_TermEmbedding();

#ifdef XPCOM_GLUE
    XPCOMGlueShutdown();
#endif

    return 1;
}

BOOL CMfcEmbedApp::OnIdle(LONG lCount)
{
    CWinApp::OnIdle(lCount);

    return FALSE;
}

void CMfcEmbedApp::OnManageProfiles()
{
#ifdef USE_PROFILES
    m_ProfileMgr->DoManageProfilesDialog(PR_FALSE);
#endif
}

void CMfcEmbedApp::OnEditPreferences()
{
    CPreferences prefs(_T("Preferences"));
    
    prefs.m_startupPage.m_iStartupPage = m_iStartupPage;
    prefs.m_startupPage.m_strHomePage = m_strHomePage;   

    if(prefs.DoModal() == IDOK)
    {
        // Update our member vars with these new pref values
        m_iStartupPage = prefs.m_startupPage.m_iStartupPage;
        m_strHomePage = prefs.m_startupPage.m_strHomePage;

        // Save these changes to disk now
        nsresult rv;
        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) 
        {
            USES_CONVERSION;
            prefs->SetIntPref("browser.startup.page", m_iStartupPage);
            rv = prefs->SetCharPref("browser.startup.homepage", T2CA(m_strHomePage));
            if (NS_SUCCEEDED(rv))
                rv = prefs->SavePrefFile(nsnull);
        }
        else
            NS_ASSERTION(PR_FALSE, "Could not get preferences service");
    }
}

BOOL CMfcEmbedApp::InitializeProfiles()
{

#ifdef MOZ_PROFILESHARING
    // If we are using profile sharing, get the sharing setup service
    nsCOMPtr<nsIProfileSharingSetup> sharingSetup =
        do_GetService("@mozilla.org/embedcomp/profile-sharing-setup;1");
    if (sharingSetup)
    {
        USES_CONVERSION;
        CString strRes;
        strRes.LoadString(IDS_PROFILES_NONSHARED_NAME);
        nsDependentString nonSharedName(T2W(strRes));
        sharingSetup->EnableSharing(nonSharedName);
    }
#endif

    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1");
    if (!observerService)
        return FALSE;

    // Both the profile mgr and standalone nsProfileDirServiceProvider
    // send this notification.
    observerService->AddObserver(this, "profile-after-change", PR_TRUE);

#ifdef USE_PROFILES
    m_ProfileMgr = new CProfileMgr;
    if (!m_ProfileMgr)
        return FALSE;

    observerService->AddObserver(this, "profile-approve-change", PR_TRUE);
    observerService->AddObserver(this, "profile-change-teardown", PR_TRUE);

    m_ProfileMgr->StartUp();
#else
    nsresult rv;
    nsCOMPtr<nsIFile> appDataDir;
    NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR,
                                getter_AddRefs(appDataDir));
    if (!appDataDir)
        return FALSE;
    nsCOMPtr<nsProfileDirServiceProvider> profProvider;
    NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(profProvider));
    if (!profProvider)
        return FALSE;
    profProvider->Register();    
    nsCOMPtr<nsILocalFile> localAppDataDir(do_QueryInterface(appDataDir));
    rv = profProvider->SetProfileDir(localAppDataDir);
    if (NS_FAILED(rv))
        return FALSE;
    NS_ADDREF(m_ProfileDirServiceProvider = profProvider);
#endif

    return TRUE;
}

// When the profile switch happens, all open browser windows need to be 
// closed. 
// In order for that not to kill off the app, we just make the MFC app's 
// mainframe be an invisible window which doesn't get closed on profile 
// switches
BOOL CMfcEmbedApp::CreateHiddenWindow()
{
    CFrameWnd *hiddenWnd = new CFrameWnd;
    if(!hiddenWnd)
        return FALSE;

    RECT bounds = { -10010, -10010, -10000, -10000 };
    hiddenWnd->Create(NULL, _T("main"), WS_DISABLED, bounds, NULL, NULL, 0, NULL);
    m_pMainWnd = hiddenWnd;

    return TRUE;
}

nsresult CMfcEmbedApp::InitializePrefs()
{
   nsresult rv;
   nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
   if (NS_SUCCEEDED(rv)) {      

        // We are using the default prefs from mozilla. If you were
        // disributing your own, this would be done simply by editing
        // the default pref files.
        
        PRBool inited;
        rv = prefs->GetBoolPref("mfcbrowser.prefs_inited", &inited);
        if (NS_FAILED(rv) || !inited)
        {
            USES_CONVERSION;
            m_iStartupPage = 1;
            m_strHomePage = "http://www.mozilla.org/projects/embedding";

            prefs->SetIntPref("browser.startup.page", m_iStartupPage);
            prefs->SetCharPref("browser.startup.homepage", T2CA(m_strHomePage));
            prefs->SetIntPref("font.size.variable.x-western", 16);
            prefs->SetIntPref("font.size.fixed.x-western", 13);
            rv = prefs->SetBoolPref("mfcbrowser.prefs_inited", PR_TRUE);
            if (NS_SUCCEEDED(rv))
                rv = prefs->SavePrefFile(nsnull);
        }
        else
        {
            // The prefs are present, read them in

            prefs->GetIntPref("browser.startup.page", &m_iStartupPage);

            nsXPIDLCString str;
            prefs->GetCharPref("browser.startup.homepage", getter_Copies(str));
            if (!str.IsEmpty())
            {
                USES_CONVERSION;
                m_strHomePage = A2CT(str.get());
            }
            else
            {
                m_strHomePage.Empty();
            }
        }       
    }
    else
        NS_ASSERTION(PR_FALSE, "Could not get preferences service");
        
    return rv;
}


/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This will be used by Gecko C++ code
   (never JS) to create new windows when no previous window is handy
   to begin with. This is done in a few exceptional cases, like PSM code.
   Failure to set this callback will only disable the ability to create
   new windows under these circumstances. */
nsresult CMfcEmbedApp::InitializeWindowCreator()
{
  // give an nsIWindowCreator to the WindowWatcher service
  nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator *, this));
  if (windowCreator) {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch) {
      wwatch->SetWindowCreator(windowCreator);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

// ---------------------------------------------------------------------------
//  CMfcEmbedApp : nsISupports
// ---------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(CMfcEmbedApp, nsIObserver, nsIWindowCreator, nsISupportsWeakReference)

// ---------------------------------------------------------------------------
//  CMfcEmbedApp : nsIObserver
// ---------------------------------------------------------------------------

// Mainly needed to support "on the fly" profile switching

NS_IMETHODIMP CMfcEmbedApp::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;
    
    if (strcmp(aTopic, "profile-approve-change") == 0)
    {
        // Ask the user if they want to
        int result = MessageBox(NULL,
            _T("Do you want to close all windows in order to switch the profile?"),
            _T("Confirm"), MB_YESNO | MB_ICONQUESTION);
        if (result != IDYES)
        {
            nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
            NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);
            status->VetoChange();
        }
    }
    else if (strcmp(aTopic, "profile-change-teardown") == 0)
    {
        // Close all open windows. Alternatively, we could just call CBrowserWindow::Stop()
        // on each. Either way, we have to stop all network activity on this phase.
        
        POSITION pos = m_FrameWndLst.GetHeadPosition();
        while( pos != NULL )
        {
            CBrowserFrame *pBrowserFrame = (CBrowserFrame *) m_FrameWndLst.GetNext(pos);
            if(pBrowserFrame)
            {
                pBrowserFrame->ShowWindow(false);

                // Passing in FALSE below so that we do not
                // kill the main app during a profile switch
                RemoveFrameFromList(pBrowserFrame, FALSE);

                pBrowserFrame->DestroyWindow();
            }
        }
    }
    else if (strcmp(aTopic, "profile-after-change") == 0)
    {
        InitializePrefs(); // In case we have just switched to a newly created profile.
        
        // Only make a new browser window on a switch. This also gets
        // called at start up and we already make a window then.
        if (!wcscmp(someData, NS_LITERAL_STRING("switch").get()))      
            OnNewBrowser();
    }
    return rv;
}

// ---------------------------------------------------------------------------
//  CMfcEmbedApp : nsIWindowCreator
// ---------------------------------------------------------------------------
NS_IMETHODIMP CMfcEmbedApp::CreateChromeWindow(nsIWebBrowserChrome *parent,
                                               PRUint32 chromeFlags,
                                               nsIWebBrowserChrome **_retval)
{
  // XXX we're ignoring the "parent" parameter
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0;

  CBrowserFrame *pBrowserFrame = CreateNewBrowserFrame(chromeFlags);
  if(pBrowserFrame) {
    *_retval = NS_STATIC_CAST(nsIWebBrowserChrome *, pBrowserFrame->GetBrowserImpl());
    NS_ADDREF(*_retval);
  }
  return NS_OK;
}

// AboutDlg Stuff

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

    enum { IDD = IDD_ABOUTBOX };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// Show the AboutDlg
void CMfcEmbedApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}
