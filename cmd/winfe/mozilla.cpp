/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "stdafx.h"

//	Various startup/shutdown functions.
void STARTUP_np(void);
void SHUTDOWN_np(void);
void STARTUP_cvffc(void);
void SHUTDOWN_cvffc(void);
#ifdef MOZ_LOC_INDEP
void STARTUP_li(void);
void SHUTDOWN_li(void);
#endif/* MOZ_LOC_INDEP */

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "jri.h"
#endif

#define XP_CPLUSPLUS  // temporary hack - jsw
//#include "secnav.h"

// XP Includes
#include "prefapi.h"
#include "NSReg.h"
#ifdef MOZ_SMARTUPDATE
#include "softupdt.h"
#endif

#ifdef XP_WIN32
#include "postal.h"    // mapi DLL interface
#endif
#include "prefinfo.h"

#ifdef XP_WIN16
#include "ctl3d.h"
#endif

// Be careful adding more headers here since doing so can force the 
// Win16 compiler to "run out of keys", whatever that means

#include "libevent.h"

// Template/Frame/View/Doc Includes

#ifdef MOZ_MAIL_NEWS
#include "wfemsg.h"
#include "addrfrm.h"
#endif /* MOZ_MAIL_NEWS */
#include "hiddenfr.h"
#include "template.h"
#include "logindg.h"
#include "widgetry.h"

extern "C"      {
	int NET_ReadCookies(char * filename);
	void NET_RemoveAllCookies();
};

// Used to simplify Gold/Non-gold template handling
BOOL bIsGold = FALSE;

#include "mainfrm.h"
#include "navfram.h"
// Misc Includes

#include "custom.h"
#include "ngdwtrst.h"
#include "oleregis.h"
#include "sysinfo.h"
#include "winproto.h"
#include "cmdparse.h"
#include "ddecmd.h"
#include "slavewnd.h"
#include "feutil.h"
#include "cxicon.h"
// Full Circle stuff - see http://www.fullsoft.com for more info
#include "fullsoft.h"

#ifdef MOZ_NGLAYOUT
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsViewsCID.h"
#endif


#if defined(OJI) || defined(JAVA)
	// don't include java.h here because the Win16 compiler won't be able to handle this file
	void WFE_LJ_StartupJava(void);
	void WFE_LJ_StartDebugger(void);
#endif

#ifdef NEW_PREF_ARCH
#include "cprofile.h"
#include "cprofmgr.h"

extern int  login_NewProfilePreInit();
extern int  login_NewProfilePostInit(CProfile *pProfile);

#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS      1
#endif

#ifdef __BORLANDC__
	#define _mkdir mkdir
#endif

extern "C" void WFE_StartCalendar();

extern "C"      {
NET_StreamClass *memory_stream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
NET_StreamClass *external_viewer_disk_stream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
NET_StreamClass *ContextSaveStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
NET_StreamClass *nfe_OleStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
NET_StreamClass *EmbedStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
NET_StreamClass *IL_ViewStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
};

extern "C" int il_debug;
extern CMapStringToOb DNSCacheMap;
extern "C" const char* FE_GetFolderDirectory(MWContext *pContext);
// Temporary hack for MUP -- jonm
extern "C" {
	HINSTANCE hAppInst;
}

extern CDocTemplate *WFE_InitializeTemplates();
#ifdef EDITOR
extern void WFE_InitComposer();
extern void WFE_ExitComposer();
#endif // EDITOR
// CLM:
//   Global message to find previous instance of program by ourselves
//    or from other applications.
//   The function OnNetscapeGoldMessage() in CGenericFrame responds TRUE
//     to this message to tell other instances or apps we are here
//
UINT WM_NG_IS_ACTIVE = 0;
UINT WM_OPEN_EDITOR = 0;
UINT WM_OPEN_NAVIGATOR = 0;
// extern XP_List* wfe_pModalDlgList;

char szNGMemoryMapFilename[] = "NGMemoryMapFileURL";


//Multi-Instance codes for detecting app exit and run status.
#define EXITING -10
#define RUNNING  10


#ifdef XP_WIN32
// System for communication between Navigator/Editor and Site Manager
// BOOL bSiteMgrIsActive = 0;
// The SiteManager client class
#ifdef EDITOR
ITalkSMClient * pITalkSMClient = NULL;
#endif // EDITOR
// This should be set ONLY if pITalkSMClient is not null
BOOL bSiteMgrIsRegistered = 0;
// We call URL Saving/Loading member functions only if
//   site manager is already active
BOOL bSiteMgrIsActive = 0;
UINT WM_SITE_MANAGER = 0;
#endif


extern "C" int SECNAV_InitConfigObject(void);
extern "C" void SECNAV_EarlyInit(void);
extern "C" void SECNAV_Init(void);
extern "C" int SECNAV_RunInitialSecConfig(void);
extern "C" void SECNAV_Shutdown(void);
#ifndef NSPR20
extern "C" PR_PUBLIC_API(void) PR_Shutdown(void);
#endif

/****************************************************************************
*
*       CONSTANTS
*
****************************************************************************/

/////////////////////////////////////////////////////////////////////////////
// CNetscapeApp

BEGIN_MESSAGE_MAP(CNetscapeApp, CWinApp)
    ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
    ON_COMMAND(ID_HELP, OnHelp)
    ON_COMMAND(ID_APP_EXIT, OnAppExit)
    ON_COMMAND(ID_APP_SUPER_EXIT, OnAppSuperExit)
END_MESSAGE_MAP()

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

// This identifier was generated to be statistically unique for your app.
// You may change it if you prefer to choose a specific identifier.
static const CLSID BASED_CODE clsid =
{ 0x61d8de20, 0xca9a, 0x11ce, { 0x9e, 0xa5, 0x0, 0x80, 0xc8, 0x2b, 0xe3, 0xb6 } };


/////////////////////////////////////////////////////////////////////////////
// The one and only CNetscapeApp object

CNetscapeApp NEAR theApp;

#ifndef MOZ_NGLAYOUT
// the only one object of CNetscapeFontModule
CNetscapeFontModule    theGlobalNSFont;
#endif /* MOZ_NGLAYOUT */

NET_StreamClass *null_stream(FO_Present_Types format_out, void *newshack, URL_Struct *urls, MWContext *cx)      {
	//      Stream which does nothing.
	return(NULL);
}

/////////////////////////////////////////////////////////////////////////////
// CNetscapeApp initialization
BOOL CNetscapeApp::InitApplication()
{
    // used to mark the beginning of the usefull stack
    // for the java gc. We used to calculate this value based on the
    // first argument to PR_Init(...) but PR_Init() has moved so
    // we are wrong now. In order to avoid keep changing the guess
    // we pass explicitly the value so wherever PR_Init() ends up we are
    // all set. The stackBase must be at this level or higher on the stack
    int stackBase;

    CWinApp::InitApplication();
	NSToolBarClass = AfxRegisterWndClass( CS_CLASSDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
					theApp.LoadStandardCursor(IDC_ARROW), 
					(HBRUSH)(COLOR_BTNFACE + 1));
    fe_InitNSPR((void*)&stackBase);
	m_appFontList = NULL;

	// Full Circle initialization
	FCInitialize();
	
    return TRUE;
}

#ifdef MOZ_NGLAYOUT
#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"
#define VIEW_DLL   "raptorview.dll"

static void InitializeNGLayout() {
  NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
  NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);
  NS_DEFINE_IID(kCHScrollbarIID, NS_HORZSCROLLBAR_CID);
  NS_DEFINE_IID(kCButtonIID, NS_BUTTON_CID);
  NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
  NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
  NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
  NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
  NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
  NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
  NS_DEFINE_IID(kCCheckButtonIID, NS_CHECKBUTTON_CID);
  NS_DEFINE_IID(kCChildIID, NS_CHILD_CID);

  NSRepository::RegisterFactory(kCWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCHScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCCheckButtonIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCChildIID, WIDGET_DLL, PR_FALSE, PR_FALSE);

  NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
  NS_DEFINE_IID(kCRegionIID, NS_REGION_CID);

  NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRegionIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

  NS_DEFINE_IID(kCViewManagerCID, NS_VIEW_MANAGER_CID);
  NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
  NS_DEFINE_IID(kCScrollingViewCID, NS_SCROLLING_VIEW_CID);

  NSRepository::RegisterFactory(kCViewManagerCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollingViewCID, VIEW_DLL, PR_FALSE, PR_FALSE);
}
#endif /* MOZ_NGLAYOUT */


typedef    FARPROC   (*WSASetBlockingHook_t) (FARPROC);
/////////////////////////////////////////////////////////////////////////////
// CNetscapeApp initialization

BOOL CNetscapeApp::InitInstance()
{
    //  Important to do this one first, as will need to service
    //      any caught messages initialization may cause.
#ifdef XP_WIN32
	BOOL BailOrStay(void);
	if(!BailOrStay())
		return FALSE;//First instance in the middle of initializing.
#endif
    slavewnd.InitInstance(m_hInstance);
    slavewnd.WindowProc(SLAVE_INITINSTANCE, 0, 0);
//PLEASE DO NOT MOVE THE ABOVE LINE.  Multiple instance detection
//relies on this window being created.

     //  We allow UNC names, to be turned off by a command line switch.
     if(IsRuntimeSwitch("-nounc")) {
	 //  Turn off UNC file names.
	 m_bUNC = FALSE;
     }

	char *prefStr;  // temporary storage space for preferences
	int32 prefInt;
	PRBool prefBool;

	// Determine whether this is a PE product ASAP!
	m_hPEInst    = LoadLibrary("muc.dll");
	m_bPEEnabled = ((UINT)m_hPEInst > 32);

	// Initialize member data
	m_pTempDir = NULL;
#ifdef XP_WIN16
    m_helpstate = NOT_SENT; //used to prevent loops in sending ID_HELP messages to the topmost windows and having them return back
#endif

	//Using resdll.dll by default
	//HINSTANCE m_hResDLLInst;
	m_hResInst = LoadLibrary("resdll.dll");
	if((UINT)m_hResInst > 32)
	{
		AfxSetResourceHandle(m_hResInst);
	}

	// Using resource dll if it's been defined and available
   /* We are no longer using the Registry and no longer need this code
   since core uses resdll.dll as well benito

	 CString szResDll = GetProfileString("INTL", "ResourceDLL", "") ;
	if (szResDll.IsEmpty() == FALSE)
	{
		m_hResInst = LoadLibrary(szResDll);
		if ((UINT)m_hResInst > 32)
		{
			char buf1[256], buf2[256];
			if (::LoadString(m_hResInst, IDS_APP_VERSION, buf1, 254) &&
			    ::LoadString(AfxGetResourceHandle(), IDS_APP_VERSION, buf2, 254))
			{
				if (strnicmp(buf1, buf2, strlen(buf1)) == 0)  // same version then use it
				{
					AfxSetResourceHandle(m_hResInst);
					FreeLibrary(m_hResDLLInst);
				}
				else
				{
					FreeLibrary(m_hResInst);
					m_hResInst = NULL;
				}
			}
			else
			{
				FreeLibrary(m_hResInst);
				m_hResInst = NULL;
			}
		}
	}
*/

	hAppInst = AfxGetResourceHandle();       // Changed so that this hack grabs resources from
						 // the Resource DLL and not the Application Instance.
						 // temporary hack for MUP -- jonm

    //  Save that we're presently in init instance, and save the initial
    //      show command so that MFC doesn't blow it away prematurely.
    m_bInInitInstance = TRUE;   //  Flag for frames created to use m_iFrameCmdShow
    m_iFrameCmdShow = m_nCmdShow;
    m_nCmdShow = SW_SHOWNORMAL;
    TRACE("Show Command is %d\n", m_iFrameCmdShow);

    //  We're not exiting yet.
    //  Set to true in OnAppExit.
    m_bExit = FALSE;

    // CLM: PREVENT LOADING MULTIPLE INSTANCES OF THE APP!

	//  Sent by the Navigator to other navigators. Used to prevent multiple
    //  instances.
    // First parse the command line to get filename and start-editor flag
	m_iCmdLnX = RuntimeIntSwitch(" /X");
	m_iCmdLnY = RuntimeIntSwitch(" /Y");
	m_iCmdLnCX = RuntimeIntSwitch(" /CX");
	m_iCmdLnCY = RuntimeIntSwitch(" /CY");

///////////////////////////////////////////////////////////////////////////
///BEGIN DDE INSTANCE CRITERIA CHECKING --SEE Abe for this stuff!!     ////
///////////////////////////////////////////////////////////////////////////
	//Determine instance case and perform component launch if applicable
	//using DDE to detect 2nd instance.


	const UINT NEAR msg_ExitStatus = RegisterWindowMessage("ExitingNetscape");

	CDDECMDWrapper *pWrapper = NULL;
	CStringList lstrArgumentList;
	CCmdParse cmdParse;

	LONG lReturn = 0;
	HWND hwnd = NULL;
	CString strServiceName;
	BOOL bAlreadyRunning = FALSE;
	strServiceName.LoadString(IDS_DDE_CMDLINE_SERVICE_NAME);

	//initialize our DDE commandline processing conversation
	DDEInitCmdLineConversation();

    CString csPrintCommand;

#ifdef XP_WIN32
	BOOL CALLBACK WinEnumProc(HWND hwnd, LPARAM lParam);

	if (hwnd = FindNavigatorHiddenWindow())
		lReturn = ::SendMessage(hwnd,msg_ExitStatus,0,0);


	//Start a conversation only if we have a
	//hidden frame up and running.  If the instance was in an exiting state
	//then let the connect to client fail.
	if (lReturn == RUNNING)
	{
		//attempt to connect to the DDE server.
		pWrapper =      CDDECMDWrapper::ClientConnect(strServiceName,
					CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ProcessCmdLine]);
	}

	if (pWrapper )
	{       //we are the client here.  we must establish a conversation with the server
		CString strDDECMDString = m_lpCmdLine;
		//make sure this always has something in it for second instance.  The first
		//instance needs something to do something for the user.  Default is start browser.
	
	if (strDDECMDString.GetLength() > 249)
	{
	    CString strDDECmdFileName = BuildDDEFile();
	    CFile fout;

	    if( !fout.Open( strDDECmdFileName, CFile::modeWrite ) )
	    {
		if (!strDDECmdFileName.IsEmpty())
		    remove(strDDECmdFileName);

		return FALSE;
	    }

	    fout.Write(strDDECMDString, strDDECMDString.GetLength());

	    strDDECMDString = "-DDEEXCEPTION" + strDDECmdFileName;
	}
	    
	    
	if (strDDECMDString.IsEmpty()) strDDECMDString = "-BROWSER";
		HCONV  hSavConv = pWrapper->m_hConv;


		HDDEDATA hData = DdeCreateDataHandle(CDDECMDWrapper::m_dwidInst,
											(LPBYTE)(const char*)strDDECMDString,
											strDDECMDString.GetLength() +1,
											0,
											0,
											CF_TEXT,
											0);

		DWORD wdResult;

		HDDEDATA ddeTransaction = DdeClientTransaction( (LPBYTE)hData,
											0xFFFFFFFF,
											hSavConv,
											0,
											0,
											XTYP_EXECUTE,
											3000,
											&wdResult);

		DdeFreeDataHandle(hData);
		DdeDisconnect(hSavConv);
		delete pWrapper;
		bAlreadyRunning = TRUE;

		return FALSE;  //EXIT THIS INSTANCE.  WE ARE DONE!!!
	}
	else
	{       //do command line stuff as usual and set flags appropriately
	csPrintCommand = RuntimeStringSwitch("/print", FALSE);
		if (ParseComponentArguments(m_lpCmdLine,TRUE))
		{
			cmdParse.Init(m_lpCmdLine,lstrArgumentList);
			if (!cmdParse.ProcessCmdLine() )
			{
				AfxMessageBox(IDS_CMDLINE_ERROR3);
				return FALSE; //make them type it in correctly
			}
		}
		parseCommandLine(m_lpCmdLine);
		//we will be the server in this case, So we must register our name service.

		//initialize our DDE commandline processing conversation.

		DdeNameService(CDDECMDWrapper::m_dwidInst,
		CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ServiceName], NULL,
		DNS_REGISTER);

	}
#else
		//We are never the client in win16 since we will never have a second instance.
	//Thus we will always start as a server.  The component launch app will decide whether
	//to execute a mozilla.exe or DDE to the mozilla.exe DDE server.

    csPrintCommand = RuntimeStringSwitch("/print", FALSE);
	if (ParseComponentArguments(m_lpCmdLine,TRUE))
	{

		//build a token list to be looked at later
		cmdParse.Init(m_lpCmdLine,lstrArgumentList);
		if (!cmdParse.ProcessCmdLine() )
		{
			AfxMessageBox(IDS_CMDLINE_ERROR3);
			return FALSE; //make them type it in correctly
		}

	}
	parseCommandLine(m_lpCmdLine);
	//we will be the server in this case, So we must register our name service.
	DdeNameService(CDDECMDWrapper::m_dwidInst,
	CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ServiceName], NULL,
	DNS_REGISTER);
#endif

///////////////////////////////////////////////////////////////////////////
///END DDE INSTANCE CRITERIA CHECKING  BLOCK                               ////
///////////////////////////////////////////////////////////////////////////



/////////COMPONENT LAUNCH

//#ifdef GOLD
	// Yes we are a gold version!
	bIsGold = TRUE;
//#endif

#ifdef EDITOR
    // Register the global messages

    //  Sent by the Navigator to other navigators. Used to prevent multiple
    //  instances.
    WM_NG_IS_ACTIVE = RegisterWindowMessage( "__lvp_str_NGIsActive__" );

    //  Sent by the Navigator to activate previous instance with URL in
    //  most recent browser window.
    WM_OPEN_EDITOR = RegisterWindowMessage( "__lvp_str_LoadUrlNav__" );

    //  Sent by the Navigator to activate previous instance with URL in
    //  a new editor window.
    WM_OPEN_NAVIGATOR = RegisterWindowMessage( "__lvp_str_LoadUrlEdt__" );

#ifdef XP_WIN32
    // Sent to identify whether window is a Navigator or Sitemanager
    WM_SITE_MANAGER = RegisterWindowMessage( "__talkSM_SM_IsAlive__" );

//#ifdef GOLD
    // Start an OLE Automation client to talk to LiveWire's Site Manager
    pITalkSMClient = new (ITalkSMClient);
    if ( pITalkSMClient && pITalkSMClient->IsRegistered()){
	// SiteManager was registered with OLE database
	bSiteMgrIsRegistered = TRUE;
	// See if we already have an instance
	//bSiteMgrIsActive = FE_FindSiteMgr();
	if( bSiteMgrIsActive ){
	    pITalkSMClient->SetKnownSMState(TRUE);
	}
    }
//#endif //GOLD
#endif //XP_WIN32
#endif //EDITOR

	TRACE("Windows 16 is %d\n", sysInfo.m_bWin16);
	TRACE("Windows 32 is %d\n", sysInfo.m_bWin32);
	TRACE("Windows 32s is %d\n", sysInfo.m_bWin32s);
	TRACE("Windows NT is %d\n", sysInfo.m_bWinNT);
	TRACE("Windows 95 is %d\n", sysInfo.m_bWin4);

	TRACE("Major version is %u\n", (unsigned)sysInfo.m_dwMajor);
	TRACE("Minor version is %u\n", (unsigned)sysInfo.m_dwMinor);
	TRACE("Build version is %u\n", (unsigned)sysInfo.m_dwBuild);

#ifdef _MSC_VER
    TRACE("Compiler version is %u\n", _MSC_VER);
#endif
#ifdef _MFC_VER
    TRACE("MFC version is %u\n", _MFC_VER);
#endif

#ifdef XP_WIN16
	// Call the magic function that feeds off the environment variable "TZ"
	// to set up the global time variables.
#ifdef __WATCOMC__
	tzset();
#else
	_tzset();
#endif
#endif // XP_WIN16



    //  Initialized OLE, or fail
    TRACE("Starting Ole\n");
    if(!AfxOleInit()) {
	AfxMessageBox(IDP_OLE_INIT_FAILED);
	return FALSE;
    }

    // Initialize 3d controls
#ifdef XP_WIN32
    Enable3dControls();
#else
	Ctl3dRegister(m_hInstance);
	Ctl3dAutoSubclass(m_hInstance);
#endif

	AssortedWidgetInit();

	JSContext * prefContext=NULL;
	JSObject *prefObject=NULL;
#ifdef NEW_PREF_ARCH
    CProfile        *profile = NULL;
    CProfileManager *profMgr;

	profile = (CProfile *) PREF_Init();
#else
	PREF_Init(NULL);

    // Read network and cache parameters

	PREF_GetConfigContext(&prefContext);
	PREF_GetPrefConfigObject(&prefObject);
	if (!(prefContext && prefObject)) return FALSE;

	if (!pref_InitInitialObjects(prefContext,prefObject))
		return FALSE;
#endif

	SECNAV_InitConfigObject();

	/*
	** Registry startup has to happen before profile stuff because creating a new
	** profile uses the registry.
	*/
#ifdef MOZ_SMARTUPDATE
    SU_Startup();
#endif
    NR_StartupRegistry();

    // Initialize the network.
    WORD wVersionRequested;
    int err;

    wVersionRequested = MAKEWORD(1,1);

    err= WSAStartup(wVersionRequested,&(sysInfo.m_wsaData));

    m_csWinsock = sysInfo.m_wsaData.szDescription;    //  Save this, used in ResolveAppName.

    if (err !=0) {
	MessageBox(NULL, szLoadString(IDS_NET_INIT_FAILED), szLoadString(AFX_IDS_APP_TITLE), MB_OK);
	return FALSE;
    }

#if defined(XP_WIN16) 
    //
    // This is a hack...  It should live in NSPR.  It will be moved for NSPR 2.0
    //
    {
	WSASetBlockingHook_t func;
	HINSTANCE hinstWinsock = LoadLibrary("winsock.dll");
	func = (WSASetBlockingHook_t)GetProcAddress(hinstWinsock, "WSASETBLOCKINGHOOK");
	ASSERT(func);
	if (func) {
	    extern BOOL PR_CALLBACK DefaultBlockingHook(void);
#if defined(DEBUG)            
	    ASSERT ( (func)( (FARPROC)DefaultBlockingHook ) );
#else
	    (func)( (FARPROC)DefaultBlockingHook );
#endif
	}
	FreeLibrary(hinstWinsock);
    }
#endif

    TRACE("Winsock description is %s\n", sysInfo.m_wsaData.szDescription);
    TRACE("Winsock status is %s\n", sysInfo.m_wsaData.szSystemStatus);

    if (LOBYTE(sysInfo.m_wsaData.wVersion) != 1 || HIBYTE(sysInfo.m_wsaData.wVersion) != 1) {
	  MessageBox(NULL, szLoadString(IDS_NET_INIT_FAILED), szLoadString(AFX_IDS_APP_TITLE), MB_OK);
	WSACleanup();
	return FALSE;
    }

    //  Determine maximum number of sockets that we will allow in the application.
    //  This is safer than just choosing some arbitrary number (always 50).
    //  Hard coded params:  Min 8 Max 50
    //  Soft coded limits:  Half of available sockets winsock reported.
    sysInfo.m_iMaxSockets = sysInfo.m_wsaData.iMaxSockets / 2;
    if(sysInfo.m_iMaxSockets < 8)   {
	sysInfo.m_iMaxSockets = 8;
    }
    else if(sysInfo.m_iMaxSockets > 50) {
	sysInfo.m_iMaxSockets = 50;
    }

    TRACE("Max number of sockets is %d\n", sysInfo.m_iMaxSockets);

    // Initialize the network module
    /* NET_InitNetLib handles socks initialization. */

    CString msg;
    //
    // Get the temp directory.  See if it exists
	// This must be before the e-kit check below
    //
    msg = GetProfileString("Main", "Temp Directory", DEF_TEMP_DIR);  // directory configured?
    if (msg != BOGUS_TEMP_DIR) {
	m_pTempDir = XP_STRDUP((const char *)msg);
    } else {
	if(getenv("TEMP"))
	  m_pTempDir = XP_STRDUP(getenv("TEMP"));  // environmental variable
	if (!m_pTempDir && getenv("TMP"))
	  m_pTempDir = XP_STRDUP(getenv("TMP"));  // How about this environmental variable?
	if (!m_pTempDir) {
	  // screw 'em put it in the windows directory
	  char path[_MAX_PATH];
	  GetWindowsDirectory(path, sizeof(path));
	  m_pTempDir = XP_STRDUP(path);
	}
    }

	//      Ensure no backslash on the end of the temp dir.
	FEU_NoTrailingBackslash(m_pTempDir);

    //
    // Check that the temp directory exists
    //
    if(!FEU_SanityCheckDir(m_pTempDir)) {

	char msg[1024];

	::sprintf(msg, szLoadString(IDS_ERR_TMP_DIR_NOT_EXIST), m_pTempDir);
	MessageBox(NULL, msg, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);

    }
	
    NET_InitNetLib(0, sysInfo.m_iMaxSockets);

	// init parts of security for li
	SECNAV_EarlyInit();

#ifdef NEW_PREF_ARCH
    // After the new preferences architecture lands from the Nova branch, enable
    // this #define


    // Get the main NSPR event queue
    mozilla_event_queue  = PR_GetMainEventQueue();

    LM_InitMocha();

	// Register all XP content type converters and stream decoders.
    // We can overwrite these settings if we have other settings ourselves.
	NET_RegisterMIMEDecoders();

    // Do PE precleanup and such
    login_NewProfilePreInit();
    
    // Read network and cache parameters

	PREF_GetConfigContext(&prefContext);
	PREF_GetPrefConfigObject(&prefObject);
	if (!(prefContext && prefObject)) return FALSE;

	if (!pref_InitInitialObjects(prefContext,prefObject))
		return FALSE;

    profile->LoadPrefs("netscape.cfg");
    profMgr = new CProfileManager(profile);

    int     profErr = PREF_OK;

    if (m_bAccountSetup) {
        // FIXME!  This isn't permanent!
//        profMgr->CreateNewDialupAcct();
        profErr = login_QueryForCurrentProfile();
    } else if (m_bCreateNewProfile) {
        if (theApp.m_bPEEnabled) {
            profErr = login_QueryForCurrentProfile();
            // FIXME!  This isn't permanent!
//            profMgr->CreateNewDialupAcct();
        } else {
            profErr = profMgr->DoNewProfileWizard(PROFMGR_DEFAULT);
        }
    } else if (m_bProfileManager) {
        profErr = profMgr->ChooseProfile(PROFMGR_FORCE_SHOW | PROFMGR_SHOW_CONTROLS, m_CmdLineProfile);
    } else if (m_CmdLineProfile != NULL) {
        profErr = profMgr->GetProfileFromName(m_CmdLineProfile);
    } else {
        if (theApp.m_bPEEnabled) {
            profErr = profMgr->ChooseProfile(PROFMGR_USE_DIALUP);
        } else {
            profErr = profMgr->ChooseProfile(PROFMGR_DEFAULT);
        }
    }

    if (profErr != PREF_OK) {
        return FALSE;
    }

    {
    char       userDir[FILENAME_MAX + 1];
    int        nameLength = FILENAME_MAX;

    profile->GetCharPref("profile.directory", userDir, &nameLength, PR_TRUE);
    theApp.m_UserDirectory = userDir;
    }

    profile->LoadPrefs();

	// do this font init here for now for ldap 
	m_iCSID = (int16)2;
	INTL_ChangeDefaultCharSetID((int16)m_iCSID);
#ifdef XP_WIN32
    theApp.m_bUseUnicodeFont = GetProfileInt("Intl", "UseUnicodeFont",  TRUE ) ;
    theApp.m_bUseVirtualFont = GetProfileInt("Intl", "UseVirtualFont",  FALSE);
    if(! theApp.m_bUseUnicodeFont ) {
	theApp.m_bUseVirtualFont = TRUE;
	}
#else
    theApp.m_bUseUnicodeFont = FALSE ;
    theApp.m_bUseVirtualFont = TRUE;
#endif

    STARTUP_cvffc();	

	// Set version and application names
    //  We have to get around it's const status.
    XP_AppName     = XP_STRDUP(szLoadString(IDS_APP_NAME));
    XP_AppLanguage = XP_STRDUP(szLoadString(IDS_APP_LANGUAGE));
    XP_AppCodeName = XP_STRDUP(szLoadString(IDS_APP_CODE_NAME));
    XP_AppVersion  = XP_STRDUP(ResolveShortAppVersion());

  // Create our hidden frame window.
	// Set this to be the applications main window.
	m_pHiddenFrame = new CHiddenFrame();
#ifdef _WIN32
	{
		WNDCLASS wc;
		wc.style         = 0;
		wc.lpfnWndProc   = ::DefWindowProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = sizeof(DWORD);               // this is used by java's setAgentPassword
		wc.hInstance     = ::AfxGetInstanceHandle();
		wc.hIcon         = NULL;
		wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
		wc.lpszMenuName  = (LPCSTR) NULL;
		wc.lpszClassName = "aHiddenFrameClass";
		ATOM result = ::RegisterClass(&wc);
		ASSERT(result != 0);
		m_pHiddenFrame->Create(wc.lpszClassName, "Netscape's Hidden Frame");
	}
#else
		m_pHiddenFrame->Create(NULL, "Netscape's Hidden Frame");
#endif
	m_pMainWnd = m_pHiddenFrame;
	m_pHiddenFrame->ShowWindow(SW_HIDE);

    do {
        profErr = login_NewProfilePostInit(profile);

        if (profErr == -1) {
            profErr = profMgr->PromptPassword();
            if (profErr == PREF_OK) {
                profErr = -1;
            }
        }
    } while (profErr == -1);

    if (profErr < PREF_OK) {
        return FALSE;
    }

    int32   csid ;
	if( PREF_GetIntPref("intl.character_set",&csid) != PREF_NOERROR)
		csid = 2;

	m_iCSID = (int16)csid;
    
//#ifdef MOZ_NETSCAPE_FONT_MODULE
	VERIFY( FONTERR_OK == theGlobalNSFont.InitFontModule() );
//#endif MOZ_NETSCAPE_FONT_MODULE

#else // NEW_PREF_ARCH
    // Create our hidden frame window.
	// Set this to be the applications main window.
	m_pHiddenFrame = new CHiddenFrame();
#ifdef _WIN32
	{
		WNDCLASS wc;
		wc.style         = 0;
		wc.lpfnWndProc   = ::DefWindowProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = sizeof(DWORD);               // this is used by java's setAgentPassword
		wc.hInstance     = ::AfxGetInstanceHandle();
		wc.hIcon         = NULL;
		wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
		wc.lpszMenuName  = (LPCSTR) NULL;
		wc.lpszClassName = "aHiddenFrameClass";
		ATOM result = ::RegisterClass(&wc);
		ASSERT(result != 0);
		m_pHiddenFrame->Create(wc.lpszClassName, "Netscape's Hidden Frame");
	}
#else
		m_pHiddenFrame->Create(NULL, "Netscape's Hidden Frame");
#endif
	m_pMainWnd = m_pHiddenFrame;
	m_pHiddenFrame->ShowWindow(SW_HIDE);

	CDocTemplate *pTemplate = WFE_InitializeTemplates();
	if (!pTemplate)
		return FALSE;

    // Get the main NSPR event queue
    mozilla_event_queue  = PR_GetMainEventQueue();

#ifndef MOZ_NGLAYOUT
    LM_InitMocha();
#endif /* MOZ_NGLAYOUT */

    // Initialize the XP file extension mapping
    NET_InitFileFormatTypes(NULL, NULL);

	// Register all XP content type converters and stream decoders.
    // We can overwrite these settings if we have other settings ourselves.
	NET_RegisterMIMEDecoders();

    /////////////////////////////////////////////////////////////////////////
    // Read in font and setup encoding table stuff
#ifdef XP_WIN32
    theApp.m_bUseUnicodeFont = GetProfileInt("Intl", "UseUnicodeFont",  TRUE ) ;
    theApp.m_bUseVirtualFont = GetProfileInt("Intl", "UseVirtualFont",  FALSE);
    if(! theApp.m_bUseUnicodeFont ) {
	theApp.m_bUseVirtualFont = TRUE;
	}
#else
    theApp.m_bUseUnicodeFont = FALSE ;
    theApp.m_bUseVirtualFont = TRUE;
#endif

	int32   csid ;
	if( PREF_GetIntPref("intl.character_set",&csid) != PREF_NOERROR)
		csid = 2;

	m_iCSID = (int16)csid;
	INTL_ChangeDefaultCharSetID((int16)csid);

#ifndef MOZ_NGLAYOUT
	STARTUP_cvffc();

	VERIFY( FONTERR_OK == theGlobalNSFont.InitFontModule() );
#endif /* MOZ_NGLAYOUT */

	// Initialize RDF
    m_pRDFCX = new CRDFCX(::RDFSlave, MWContextRDFSlave);
	
    // Set version and application names
    //  We have to get around it's const status.
    XP_AppName     = XP_STRDUP(szLoadString(IDS_APP_NAME));
    XP_AppLanguage = XP_STRDUP(szLoadString(IDS_APP_LANGUAGE));
    XP_AppCodeName = XP_STRDUP(szLoadString(IDS_APP_CODE_NAME));
    XP_AppVersion  = XP_STRDUP(ResolveShortAppVersion());

	if (!login_QueryForCurrentProfile())
		return FALSE;

#endif  // NEW_PREF_ARCH

	// Read in font and setup encoding table stuff (must be done after prefs are read)
	m_pIntlFont = new CIntlFont;

    // SECNAV_INIT requires AppCodeName and the first substring in AppVersion 
    SECNAV_Init();

	GH_InitGlobalHistory();

	NET_RemoveAllCookies();
	NET_ReadCookies("");

	PREF_GetIntPref("network.tcpbufsize",&prefInt);
    int nTCPBuff = CASTINT(prefInt);

    //  Have preference watchers commence since prefs are up.
    prefInfo.Initialize();

    //move to this location due to report that PREF calls were crashing 16bit java.
	//also, only do this if we are the first instance.  See the instance checing section
	//for where bAlreadyRunning is set.  ifdef'd for win16 since never have 2 instances.
#ifdef XP_WIN32
    if(!bAlreadyRunning)
	{
#endif
#ifdef MOZ_OFFLINE
		extern void AskMeDlg(void);
		AskMeDlg();	
#endif /* MOZ_OFFLINE */
#ifdef XP_WIN32
    }
#endif
	//      If we're running automated or embedded, we instantly need to create
	//              a new lock on the appication.
	//      So OnIdle for a full description of the problem at hand.
    //  Consider ourselves automated if we have a print command.
    m_bEmbedded = RunEmbedded();
    m_bAutomated = RunAutomated();

    // register the icon for our document files
    auto char ca_default[_MAX_PATH + 5];
    ::GetModuleFileName(m_hInstance, ca_default, _MAX_PATH);
    strcat(ca_default, ",1"); //Dave: Fixed this
    if(!fe_RegisterOLEIcon("{481ED670-9D30-11ce-8F9B-0800091AC64E}",
			"Netscape Hypertext Document",
			ca_default)) {
	TRACE("Failed to register OLE Icons\n");
    }


	if (!(sysInfo.m_bWin32 && sysInfo.m_dwMajor >= 4))
	SetDialogBkColor();    // set dialog background color to gray
    LoadStdProfileSettings();  // Load standard INI file options (including MRU)

    hAccelTable = LoadAccelerators (m_hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));

	// Enable shell printing
	EnableShellStuff();

    //  Get the list of trusted user applications.
    m_pSpawn = new CSpawnList();

    // The first order of business is to get our install directory so that we can
    //    locate the dumb files and use the install dir as the default for other
    //    files and directories

    if (!strcmpi(szLoadString(IDS_LOCKED_PREFERENCES),"yes"))
	m_bUseLockedPrefs = TRUE;
    else
	m_bUseLockedPrefs = FALSE;

#ifndef MOZ_NGLAYOUT
    // Frame creation may cause the loading of the home page so register
    // all of the parser and network functions first
    static PA_InitData parser_data;
    parser_data.output_func = LO_ProcessTag;
#endif /* MOZ_NGLAYOUT */

	PREF_GetIntPref("network.max_connections",&prefInt);
    int nMaxConnect = CASTINT(prefInt);

    // In 2.0 this has no GUI and is only settable via the installer for
    //   Personal Edition so don't put it in the m_PrefList list so that
    //   we don't create the entry when we flush out all of our preferences
//    CPref nTCPTimeOut(NULL, "Network", "TCP Connect Timeout", 0);

      /* larubbio begin */
    msg = m_UserDirectory;
    msg += "\\archive";

    m_pSARCacheDir = msg;

    //
    // make sure we can write to this directory
    //
    if(!FEU_SanityCheckDir(m_pSARCacheDir)){

	char msg[1024];
	::sprintf(msg, szLoadString(IDS_ERR_CACHE_DIR_NOT_EXIST), m_pSARCacheDir);
	MessageBox(NULL, msg, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);

    }
      /* larubbio end */

    // Read the cache dir
	msg = m_UserDirectory;
    msg += "\\cache";
	PREF_SetDefaultCharPref("browser.cache.directory",msg);

	prefStr = NULL;
	PREF_CopyCharPref("browser.cache.directory",&prefStr);
	m_pCacheDir = prefStr;
	PREF_RegisterCallback("browser.cache.directory",WinFEPrefChangedFunc,NULL);
	if (prefStr) XP_FREE(prefStr);

    //
    // make sure we can write to this directory
    //
	XP_Bool bTurnOffDiskCache = FALSE;

    if(!FEU_SanityCheckDir(m_pCacheDir)){

	char msg[1024];
	::sprintf(msg, szLoadString(IDS_ERR_CACHE_DIR_NOT_EXIST), m_pCacheDir);
	MessageBox(NULL, msg, szLoadString(AFX_IDS_APP_TITLE), MB_ICONEXCLAMATION | MB_OK);

	// turn off the cache for now
	bTurnOffDiskCache = TRUE;
    }

#ifdef JEM
    // Initialize the network module
    /* NET_InitNetLib handles socks initialization. */
    NET_InitNetLib(nTCPBuff, sysInfo.m_iMaxSockets);
#endif

#ifdef MOZ_LOC_INDEP
	STARTUP_li();
#endif /* MOZ_LOC_INDEP */

    SECNAV_RunInitialSecConfig();

    NET_ChangeMaxNumberOfConnectionsPerContext(nMaxConnect);

    // throw away old wrong app version, get right one */
    XP_FREE(XP_AppVersion);
    XP_AppVersion = XP_STRDUP(ResolveAppVersion());

    if (bTurnOffDiskCache) NET_SetDiskCacheSize(0);

    //
    // Only mess with the TCP connect time out if we have gotten a
    //   positive number out of the Registry/INI file.  Don't let the
    //   number go below 30 seconds or above 4 minutes
    //
    int32 nTCPTimeOut;
    PREF_GetIntPref("network.tcptimeout",&nTCPTimeOut);
    if(nTCPTimeOut > 30 && nTCPTimeOut < 240)
	NET_SetTCPConnectTimeout(nTCPTimeOut);
    else if(nTCPTimeOut > 240)
	NET_SetTCPConnectTimeout(240);

#if defined(XP_WIN32)
    XP_AppPlatform = "Win32";
#elif defined(XP_WIN16)
    XP_AppPlatform = "Win16";
#else
#error Unexpected platform!
#endif


    // Get the Id of the message which NSPR uses for event notifications...
    m_msgNSPREventNotify = ::RegisterWindowMessage("NSPR_PostEvent");

  // rhp - Added change for not showing splash screen for MAPI startup
    // now that we are done with everything that can fail, show the splash
	if (showSplashScreen(csPrintCommand)) 
	{
		// only show splash if not embedded
	m_splash.Create(NULL);
#if !defined(DEBUG_chouck) && !defined(DEBUG_warren) && !defined(DEBUG_phil) && !defined(DEBUG_hyatt)
	m_splash.ShowWindow(SW_SHOW);
#endif
	m_splash.UpdateWindow();
	}

    // figure out if we need to have a little N in the toolbar for
    //  cobranding purposes
	int iTmp;
	m_bShowNetscapeButton = CUST_IsCustomAnimation(&iTmp);



//BEGIN STREAM VODOO


    // Add user configured MIME types and file extensions to the NETLIB lists.
    // This sets up any user configured viewers also, by placing them in a list that
    //      will be entered in the function external_viewer_disk_stream....  What a kludge.
    // This also constructs a list of possible helper applications that are spawned off
    //      in external_viewer_disk_stream read in from the INI file.
    fe_InitFileFormatTypes();

#ifdef XP_WIN32
	// Check to see if we're the "default browser"
	CheckDefaultBrowser();
#endif // XP_WIN32

	//Begin CRN_MIME
	//At this point we have all the mime_type prefs(from prefs.js, netscape.cfg) 
	//read into the prefs table. Use this info to update the NETLIB lists.
	fe_UpdateMControlMimeTypes(); 

	//Register a callback so that we can update the mime type info., in response to 
	//new AutoAdmin prefs updates.
	PREF_RegisterCallback("mime.types.all_defined",WinFEPrefChangedFunc,NULL);
	//End CRN_MIME

    //  NEVER MODIFY THE BELOW unless NEW FO types appear.
    //  NEVER CALL RealNET_RegisterContentTypeConverter ANYWHERE ELSE (exceptions in presentm.cpp).
    //  INSTEAD USE NET_RegisterContentTypeConverter.
    //  WE MUST INITIALIZE EVERY FO_Format_Out TO OUR PRESENTATION MANAGER.
    char *cp_wild = "*";
    //  Mocha src equal converter.
    NET_RegisterContentTypeConverter("application/x-javascript", FO_PRESENT, NULL, NET_CreateMochaConverter);

	// XXX dkarlton is adding preencrypted files here, even though it's called
    // in libnet/cvmime.c. What's going on here?
#ifdef FORTEZZA
    NET_RegisterContentTypeConverter(INTERNAL_PRE_ENCRYPTED, FO_PRESENT, NULL, SECNAV_MakePreencryptedStream);
// mwh - xxx need to check on this.
    //NET_RegisterContentTypeConverter(INTERNAL_PRE_ENCRYPTED, FO_INTERNAL_IMAGE, NULL, PresentationManagerStream);
    //NET_RegisterContentTypeConverter(INTERNAL_PRE_ENCRYPTED, FO_VIEW_SOURCE, NULL, PresentationManagerStream);
    NET_RegisterContentTypeConverter(APPLICATION_PRE_ENCRYPTED, FO_PRESENT, NULL, SECNAV_MakePreencryptedStream);
// mwh - xxx need to check on this.
    //NET_RegisterContentTypeConverter(APPLICATION_PRE_ENCRYPTED, FO_INTERNAL_IMAGE, NULL, PresentationManagerStream);
    //NET_RegisterContentTypeConverter(APPLICATION_PRE_ENCRYPTED, FO_VIEW_SOURCE, NULL, PresentationManagerStream);
#endif

#ifdef JAVA
    //NET_RegisterContentTypeConverter(cp_wild, FO_DO_JAVA, NULL, PresentationManagerStream);
#endif

#ifdef FORTEZZA
    NET_RegisterContentTypeConverter(APPLICATION_PRE_ENCRYPTED, FO_CACHE_AND_PRESENT, NULL, SECNAV_MakePreencryptedStream);
    NET_RegisterContentTypeConverter(APPLICATION_PRE_ENCRYPTED, FO_CACHE_AND_INTERNAL_IMAGE, NULL, SECNAV_MakePreencryptedStream);
    NET_RegisterContentTypeConverter(APPLICATION_PRE_ENCRYPTED, FO_CACHE_AND_VIEW_SOURCE, NULL,SECNAV_MakePreencryptedStream);
#endif

    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_ONLY, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_PRESENT, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_INTERNAL_IMAGE, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_OUT_TO_PROXY_CLIENT, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_VIEW_SOURCE, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_SAVE_AS, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_SAVE_AS_TEXT, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_SAVE_AS_POSTSCRIPT, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_QUOTE_MESSAGE, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_MAIL_TO, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_OLE_NETWORK, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_MULTIPART_IMAGE, NULL, NET_CacheConverter);
    NET_RegisterContentTypeConverter(cp_wild, FO_CACHE_AND_PRINT, NULL, NET_CacheConverter);

    // Set up converters for our Presentation Manager.
    // Front end sets these up, only because the front end handles front end converters!
    // XP code is responsible for registering their proper converters in NET_RegisterMIMEDecoders
    NET_RegisterContentTypeConverter("text/*", FO_VIEW_SOURCE, NULL, memory_stream);      // */
    NET_RegisterContentTypeConverter(cp_wild, FO_PRESENT, NULL, external_viewer_disk_stream);
    NET_RegisterContentTypeConverter(cp_wild, FO_PRINT, NULL, external_viewer_disk_stream);
    NET_RegisterContentTypeConverter(cp_wild, FO_VIEW_SOURCE, NULL, memory_stream);
    NET_RegisterContentTypeConverter(cp_wild, FO_OUT_TO_PROXY_CLIENT, NULL, external_viewer_disk_stream);
    NET_RegisterContentTypeConverter(cp_wild, FO_SAVE_AS, NULL, ContextSaveStream);
    NET_RegisterContentTypeConverter(cp_wild, FO_OLE_NETWORK, NULL, nfe_OleStream);
#ifndef MOZ_NGLAYOUT
	NET_RegisterContentTypeConverter(cp_wild, FO_EMBED, NULL, EmbedStream);
#endif
	//      added by ftang & jliu, just remap it from memory_stream->net_ColorHTMLStream
    NET_RegisterContentTypeConverter(INTERNAL_PARSER, FO_VIEW_SOURCE, TEXT_HTML, net_ColorHTMLStream);

    //  EVERY FO_PRESENT REGISTRATION NEEDS TO BE ALSO DONE FOR FO_PRINT!!!
    //  Need to handle a format out of FO_PRINT in order to correctly print without spawning
    //      registered external/automated viewers.
    //  Had to look in odd places to find all the correct ones.  Try cvmime.c
    NET_RegisterContentTypeConverter(TEXT_HTML, FO_PRINT, NULL, INTL_ConvCharCode);
    NET_RegisterContentTypeConverter(TEXT_MDL, FO_PRINT, NULL, INTL_ConvCharCode);
    NET_RegisterContentTypeConverter(TEXT_PLAIN, FO_PRINT, NULL, NET_PlainTextConverter);
    NET_RegisterContentTypeConverter(UNKNOWN_CONTENT_TYPE, FO_PRINT, NULL, NET_PlainTextConverter);
#ifndef MOZ_NGLAYOUT
    NET_RegisterContentTypeConverter(INTERNAL_PARSER, FO_PRINT, (void *)&parser_data, PA_BeginParseMDL);
#endif /* MOZ_NGLAYOUT */
    NET_RegisterContentTypeConverter(IMAGE_GIF, FO_PRINT, NULL, IL_ViewStream);
    NET_RegisterContentTypeConverter(IMAGE_XBM, FO_PRINT, NULL, IL_ViewStream);
    NET_RegisterContentTypeConverter(IMAGE_JPG, FO_PRINT, NULL, IL_ViewStream);

    // Don't handle printing cases if we can't format it.
//    NET_RegisterContentTypeConverter(cp_wild, FO_PRINT, NULL, null_stream);

    //  Initialize our OLE viewers in WPM.
    COleRegistry::RegisterIniViewers();
    //  Initialize our OLE protocol handlers.
    COleRegistry::RegisterIniProtocolHandlers();
//END STREAM VODOO


#ifdef MOZ_NGLAYOUT
  InitializeNGLayout();
#endif

	CString strStatus;

  // rhp - Added flag for MAPI startup...
	if (showSplashScreen(csPrintCommand)) {
		strStatus.LoadString(IDS_LOAD_PREFS);
		m_splash.DisplayStatus(strStatus);
	}

	StoreVersionInReg();

    // BUG: will need to make sure this directory exists
    /////////////////////////////////////////////////////////////////////////
    // Look up application level preferences

    // Override Home Page for PE ASW Entry here??  SWE

    if (!theApp.m_csPEPage.IsEmpty())
    {
	m_pHomePage = (const char *) theApp.m_csPEPage;
	// Kludge this one too to make it go there...
	m_CmdLineLoadURL = XP_STRDUP((const char *) theApp.m_csPEPage);
    }
    else
    {
    // Read the home page out of the .ini file
	prefStr = NULL;
	if (!PREF_CopyCharPref("browser.startup.homepage",&prefStr)) {
		m_pHomePage = prefStr;
		if (prefStr) XP_FREE(prefStr);
	} else m_pHomePage = "";
	PREF_RegisterCallback("browser.startup.homepage",WinFEPrefChangedFunc,NULL);
    }

	// TODO (jonm) register WFE pref callbacks

    if(!strcmp(szLoadString(IDS_CHANGE_HOMEPAGE), "yes")) {
		m_nChangeHomePage = TRUE;
    } else {
		PREF_SetCharPref("browser.startup.homepage",szLoadString(IDS_DEF_HOMEPAGE));
		m_nChangeHomePage = FALSE;
    }

	fe_InitJava();

    /////////////////////////////////////////////////////////////////////////
	// what menu were we last looking at in the preferences window?
    m_nConfig = GetProfileInt("Main", "Last Config Menu", 0);


     /////////////////////////////////////////////////////////////////////
    // Security warnings
    for(int i = 0; i < MAX_SECURITY_CHECKS; i++)
	m_nSecurityCheck[i] = TRUE;

	if(GetProfileString("Security", "Warn Entering", "yes") == "no")
		m_nSecurityCheck[SD_ENTERING_SECURE_SPACE] = FALSE;
	if(GetProfileString("Security", "Warn Leaving", "yes") == "no")
		m_nSecurityCheck[SD_LEAVING_SECURE_SPACE] = FALSE;
	if(GetProfileString("Security", "Warn Mixed", "yes") == "no")
		m_nSecurityCheck[SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN] = FALSE;
	if(GetProfileString("Security", "Warn Insecure Forms", "yes") == "no")
		m_nSecurityCheck[SD_INSECURE_POST_FROM_INSECURE_DOC] = FALSE;


#ifdef EDITOR
    // Initialize any global data etc. needed by composer
    // (Implemented in EDFRAME.CPP)
    WFE_InitComposer();
#endif

    /////////////////////////////////////////////////////////////////////
    // Protocol Whining Preferences
	prefBool=TRUE;
	PREF_GetBoolPref("security.submit_email_forms",&prefBool);
	NET_WarnOnMailtoPost((PRBool)prefBool);

    m_pTelnet = "";
    m_pTn3270 = "";
    m_pHTML = "";

	//
	// Initialize message stuff...
	//
#ifdef MOZ_MAIL_NEWS
	WFE_MSGInit();
#endif /* MOZ_MAIL_NEWS */   

  // rhp - Added flag for MAPI startup...
	if (showSplashScreen(csPrintCommand)) {
		strStatus.LoadString(IDS_LOAD_BOOKMARKS);
		m_splash.DisplayStatus(strStatus);
	}

// RDF INITIALIZATION (BEGINS HERE)
	CString profileDirURL;
	WFE_ConvertFile2Url(profileDirURL, theApp.m_UserDirectory);
	profileDirURL = profileDirURL + "/";
	CString encodedDir;
	encodedDir = WFE_EscapeURL(profileDirURL);

	RDF_InitParamsStruct initStruct;
	initStruct.profileURL = (char *)(const char*)(encodedDir);
	
	//m_RDFStdResources = RDF_StdVocab();

    m_bInInitInstance = FALSE;

	msg = m_UserDirectory + "\\bookmark.htm";
	PREF_SetDefaultCharPref("browser.bookmark_location",msg);

	CString bookmarkFile;
	prefStr = NULL;
	PREF_CopyCharPref("browser.bookmark_location",&prefStr);
	bookmarkFile = prefStr;
	if (prefStr) XP_FREE(prefStr);
		
	CString urlHistory = profileDirURL + "mozilla.hst"; 

	// Encode the bookmarks
	CString urlBookmark;
	WFE_ConvertFile2Url(urlBookmark, bookmarkFile);
	CString encodedBookmark = WFE_EscapeURL(urlBookmark);

	initStruct.bookmarksURL = (char*)(const char*)encodedBookmark;
	initStruct.globalHistoryURL = (char*)(const char*)urlHistory;

	RDF_Init(&initStruct);	// TODO: Can this fail? Want to bail if so.
	

// RDF INITIALIZATION ENDS HERE

    m_bInInitInstance = FALSE;

    // read in bookmark location
//    msg = m_pInstallDir->GetCharValue();
	
	prefStr = NULL;
	PREF_CopyCharPref("browser.bookmark_location",&prefStr);
    m_pBookmarkFile = prefStr;

	if (prefStr) XP_FREE(prefStr);
	PREF_RegisterCallback("browser.bookmark_location",WinFEPrefChangedFunc,NULL);

    if(_access(m_pBookmarkFile,0)==-1) {
		PREF_SetCharPref("browser.bookmark_location",msg);
	}

#ifdef MOZ_OFFLINE
	m_bSynchronizingExit = FALSE;
	m_bSynchronizing = FALSE;
#endif/* MOZ_OFFLINE */

	// Get LDAP servers filename
#ifdef MOZ_LDAP
	msg = m_UserDirectory;
    msg += "\\ldapsrvr.txt";
	PREF_SetDefaultCharPref("browser.ldapfile_location",msg);

	char * ldapFile = NULL;
	PREF_CopyCharPref("browser.ldapfile_location",&ldapFile);

	// Initialize the directories and PAB
    msg = "abook.nab";
	PREF_SetDefaultCharPref("browser.addressbook_location",msg);

	CFileException e;
	CStdioFile serversFile;
	if (serversFile.Open(ldapFile,
				CFile::modeRead | CFile::typeText, &e) &&
		serversFile.GetLength())
	{
		char text[1024], temp[256];
		int pos = 0;
		int line = 0;
		m_directories = XP_ListNew();
		while (serversFile.ReadString(text, 1023) != NULL)
		{
			DIR_Server* dir = (DIR_Server *) XP_ALLOC(sizeof(DIR_Server));  // alloc new struct
			if (dir)
			{
				DIR_InitServer(dir);
				if (line)
					dir->dirType = LDAPDirectory;   /* required                                                     */
				else
				{
					dir->dirType = PABDirectory;    /* required                                                     */
					dir->fileName = XP_STRDUP(msg);
				}
				line += 1;
				int pos = 0;
				int tempPos = 0;
				int total = 0;
				int type = 0;
				while (text[pos] != '\0')
				{
					while (text[pos] != '\t' && text[pos] != '\0')
					{
						temp[tempPos++] = text[pos++];
					}
					temp[tempPos] = '\0';
					switch (type++)
					{
					case 0:
						dir->description =  XP_STRDUP(temp);
						break;
					case 1:
						dir->serverName =  XP_STRDUP(temp);
						break;
					case 2:
						dir->searchBase =  XP_STRDUP(temp);
						break;
					case 3:
						dir->port = atoi(temp);
						break;
					case 4:
						dir->maxHits =  atoi(temp);
						break;
					case 5:
						dir->isSecure =  atoi(temp);
						break;
					case 6:
						dir->savePassword =  atoi(temp);
						break;
					default:
						break;
					}
					tempPos = 0;
					if (text[pos] == '\t')
						pos++;

				}

				if (dir->dirType == LDAPDirectory)
					dir->fileName = WH_FileName (dir->serverName, xpAddrBook);

				XP_ListAddObjectToEnd(m_directories, dir);
			}
		}
		serversFile.Close();
		if (DIR_SaveServerPreferences (m_directories) != -1)
			CFile::Remove( ldapFile );
	}
	else
	{
		m_directories = DIR_GetDirServers();
		char * ldapPref = PR_smprintf("ldap_%d.end_of_directories", kCurrentListVersion);
		if (ldapPref)
			PREF_RegisterCallback(ldapPref, DirServerListChanged, NULL);
		XP_FREEIF (ldapPref);
	}
	if (ldapFile) XP_FREE(ldapFile);

#ifndef MOZ_NEWADDR
	DIR_Server* pab = NULL;

	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
	XP_ASSERT (pab);

	msg = m_UserDirectory;
    msg += "\\address.htm";
	AB_InitializeAddressBook(pab, &m_pABook, (const char *) msg);
#endif
#endif /* MOZ_LDAP */

    //  Initialize slave URL loading context.
    //  Use HTML help IDs, per legacy slave context code
    //      in olehelp.cpp
    m_pSlaveCX = new CStubsCX(::HtmlHelp, MWContextHTMLHelp);

#ifdef MOZ_MAIL_NEWS
    // Initialize the address book context
	CStubsCX *pCx = new CAddrCX();
    m_pAddressContext = pCx->GetContext();
#endif /* MOZ_MAIL_NEWS */

  // rhp - Added flags for MAPI startup...
	if (showSplashScreen(csPrintCommand)) {
		strStatus.LoadString(IDS_LOAD_PLUGINS);
		m_splash.DisplayStatus(strStatus);
	}

	STARTUP_np();

	// if PE mode, start up java too!
#if defined(OJI) || defined(JAVA)
	if (m_bAccountSetupStartupJava) {

    // rhp - Added flags for MAPI startup...
    if (showSplashScreen(csPrintCommand)) {
			strStatus.LoadString(IDS_LOAD_JAVA);
			m_splash.DisplayStatus(strStatus);
		}

		WFE_LJ_StartupJava();   // stub call because java.h can't be included here

	}
#endif

    //  right before we take down the splash screen, do our OLE startup notifications.
    COleRegistry::Startup();

    //CLM: Placed here as done in AKBAR
    // now that we are done with everything, kill the splash
    // rhp - Added flag for MAPI startup...
	if (showSplashScreen(csPrintCommand)) {
		// only show splash if not embedded
		m_splash.NavDoneIniting();
    }
#if defined(OJI) || defined(JAVA)
	// if PE mode, check if java environment is valid
	if (m_bAccountSetupStartupJava) {

#ifdef OJI
        JRIEnv* ee = NULL;
        JVMMgr* jvmMgr = JVM_GetJVMMgr();
        if (jvmMgr) {
            NPIJVMPlugin* jvm = jvmMgr->GetJVM();
            if (jvm) {
                ee = jvm->EnsureExecEnv();
                jvm->Release();
            }
            jvmMgr->Release();
        }
#else
        JRIEnv * ee = JRI_GetCurrentEnv();
#endif
        if (ee == NULL)  {
            CString szJavaStartupErr = "You are starting up an application that needs java.\nPlease turn on java in your navigator's preference and try again.";
            AfxMessageBox(szJavaStartupErr, MB_OK);
            m_bAccountSetupStartupJava = FALSE;
            return FALSE;
        }
        m_bAccountSetupStartupJava = FALSE;
	}
#endif
    // Its now safe to create the frame objects and start this whole thang
	//      only if we're not automated.
    //
	//
	/// For adding of AddressBook frame to frame list.non private version.
	void    WFE_AddNewFrameToFrameList(CGenericFrame * pFrame);

	//Please make sure this code stays before the startupMode stuff so that
	//we can process the StartMode commands in AltMail mode
	prefBool = FALSE;
	PREF_GetBoolPref("mail.use_altmail",&prefBool);
    if(prefBool)
	FEU_OpenMapiLibrary();

	if(m_bAutomated == FALSE && m_bEmbedded == FALSE  && csPrintCommand.IsEmpty())
	{
		long iStartupMode=0;
	    PRBool bStartMode = FALSE;

		PREF_GetBoolPref("general.startup.browser", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_BROWSER;

		PREF_GetBoolPref("general.startup.mail", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_MAIL;

		PREF_GetBoolPref("general.startup.calendar", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_CALENDAR;

		PREF_GetBoolPref("general.startup.news", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_NEWS;

		PREF_GetBoolPref("general.startup.editor", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_EDITOR;

		PREF_GetBoolPref("general.startup.netcaster", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_NETCASTER;

        PREF_GetBoolPref("general.startup.calendar", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_CALENDAR;

        PREF_GetBoolPref("general.startup.conference", &bStartMode);
		if (bStartMode)
			iStartupMode |= STARTUP_CONFERENCE;

		//over ride prefference settings on iStartupMode if necessary
		SetStartupMode(&iStartupMode);

		//Last chance to make sure at least the browser is launched if nothing else
		//was set either in preferences or on the command line.
		//--- Added STARTUP_ACCOUNT_SETUP here because we want browser in main window and error in DDE
		if (iStartupMode == 0 || iStartupMode == STARTUP_ACCOUNT_SETUP)
			iStartupMode |= STARTUP_BROWSER;

    void OpenDraftExit (URL_Struct *url_struct, int/*status*/,MWContext *pContext);

	if (!m_bKioskMode)
		{

			// So, instead of adding to the nightmarish way of launching components
			// below, Netcaster will look for the set bit BEFORE the switch statement,
			// launch itself, then remove the bit.  In an absurd kind of way, this seems
			// cleaner than adding a boatload of new #defines to support a fifth module.
			if (iStartupMode & STARTUP_NETCASTER && FE_IsNetcasterInstalled()) {
				FEU_OpenNetcaster() ;
				(iStartupMode &= (~STARTUP_NETCASTER)) ;
			}

            if (iStartupMode & STARTUP_CALENDAR) {
				CString installDirectory, executable;
#ifdef WIN32
    	        CString calRegistry;

	            calRegistry.LoadString(IDS_CALENDAR_REGISTRY);

	            calRegistry = FEU_GetCurrentRegistry(calRegistry);
	            if(!calRegistry.IsEmpty())
                {

                  installDirectory = FEU_GetInstallationDirectory(calRegistry, szLoadString(IDS_INSTALL_DIRECTORY));
	              executable = szLoadString(IDS_CALENDAR32EXE);
#else ifdef XP_WIN16
                  installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_CALENDAR),szLoadString(IDS_INSTALL_DIRECTORY));
	              executable = szLoadString(IDS_CALENDAR16EXE);
#endif
                  if(!installDirectory.IsEmpty())
                  {
		            executable = installDirectory + executable;

        		    WinExec(executable, SW_SHOW);
		    	   
                  }
#ifdef WIN32	           
                }
#endif
                
                (iStartupMode &= (~STARTUP_CALENDAR)) ;
			}

            if (iStartupMode & STARTUP_CONFERENCE) {
				CString installDirectory, executable;

#ifdef WIN32
                installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_CONFERENCE_REGISTRY), szLoadString(IDS_PATHNAME));
	            executable = "\\nsconf32.exe -b";
#else ifdef XP_WIN16
                installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_CONFERENCE),szLoadString(IDS_INSTALL_DIRECTORY));
            	executable = "\\nsconf16.exe -b";
#endif
 
	            if(!installDirectory.IsEmpty())
	            {
		            executable = installDirectory + executable;

		            WinExec(executable, SW_SHOW);
	            }
				(iStartupMode &= (~STARTUP_CONFERENCE)) ;
			}

			//command line component launch
			switch(iStartupMode)
			{
                case STARTUP_NETHELP: {
                    //  Have NetHelp handle the request.
                    ASSERT(m_CmdLineLoadURL);  // Okay, who messed with it?
                    if(m_pSlaveCX) {
                        m_pSlaveCX->NormalGetUrl(m_CmdLineLoadURL);
                    }
                    
                    //  Get rid of the URL.
                    XP_FREE(m_CmdLineLoadURL);
                    m_CmdLineLoadURL = NULL;
                    break;
                }

				case STARTUP_CALENDAR:
					WFE_StartCalendar();
					break;

#ifdef MOZ_MAIL_NEWS
				case STARTUP_INBOX:
					WFE_MSGOpenInbox();     //opens the inbox folder with the split pain view.
					break;

        case STARTUP_CLIENT_MAPI:  // rhp - for MAPI Startup
        case STARTUP_CLIENT_ABAPI: // rhp - for Address Book API
          {
            CGenericDoc *pDoc = (CGenericDoc *)theApp.m_ViewTmplate->OpenDocumentFile(NULL, FALSE);
            if (pDoc)
            {
              CFrameGlue *pFrame = (pDoc->GetContext())->GetFrame();
		          if(pFrame)
              {
        			  CFrameWnd *pFrameWnd = pFrame->GetFrameWnd();
                if (pFrameWnd)
                {
                  extern void StoreMAPIFrameWnd(CFrameWnd *pFrameWnd);
                  extern void StoreNABFrameWnd(CFrameWnd *pFrameWnd);

                  pFrameWnd->ShowWindow(SW_SHOWMINIMIZED);
                  if (iStartupMode == STARTUP_CLIENT_MAPI)
                  {
                    StoreMAPIFrameWnd(pFrameWnd);
                  }
                  else
                  {
                    StoreNABFrameWnd(pFrameWnd);
                  }
                }
              } 
            }
          }   
          // rhp - for MAPI startup...
          break;

				case STARTUP_FOLDER:
					WFE_MSGOpenInbox();     
					break;

				case STARTUP_MAIL:
					WFE_MSGOpenInbox();     //opens the inbox folder with the split pain view.
					//tell the browser to open commandline URL even though MAIL is set in preferences
					if(m_CmdLineLoadURL != NULL)
						iStartupMode |= STARTUP_BROWSER;
					break;

				case STARTUP_FOLDERS:
					WFE_MSGOpenFolders();           //opens the collections window
					break;

				case STARTUP_COMPOSE:
					if(!lstrArgumentList.GetCount())
						MSG_Mail(m_pBmContext); // starts the mail composition window.
					else
					{       //handle any extra arguments for this flag here.
						char *pTemp;
						URL_Struct* pURL;
						CString strUrl;

						int nArgCount = lstrArgumentList.GetCount();
						if (nArgCount == 1)
			{//either a message or an attachment.  If it's an attachment it 
			 //will be prefixed with an "@" symbol
						    pTemp  = (char*) (const char*)lstrArgumentList.GetHead();
			    //decide whether it's a message or an attachment
			    if(*pTemp != '@' && *pTemp != '-') 
			    {   //it's a message
								CFileStatus rStatus;
						if(CFile::GetStatus(pTemp,rStatus))
								{
						    WFE_ConvertFile2Url(strUrl, pTemp);
								    pURL = NET_CreateURLStruct(strUrl, NET_DONT_RELOAD);
				    StrAllocCopy(pURL->content_type,"message/rfc822");
									NET_GetURL (pURL, FO_OPEN_DRAFT, m_pBmContext,
								    OpenDraftExit);
									//Load it into the compose window
								}
								else
								{
									AfxMessageBox(IDS_CMDLINE_ERROR1);
									MSG_Mail(m_pBmContext);
				    //were just going to act as if we couldn't read what they gave us and continue
								}
							}
							else
			    {//it's an attachment
								pTemp += 1;
								CFileStatus rStatus;
						if(CFile::GetStatus(pTemp,rStatus))
								{
									if (!LaunchComposeAndAttach(lstrArgumentList))
									{
										AfxMessageBox(IDS_CMDLINE_ERROR3);
										MSG_Mail(m_pBmContext);
									}
							//Load it into the compose window
								}
								else
								{
									AfxMessageBox(IDS_CMDLINE_ERROR1);
								    MSG_Mail(m_pBmContext);
								}
							}
						}
						else
						{
						    if (nArgCount == 2)
						    {
				//it's a message and has an attachment(s)
								if (!LaunchComposeAndAttach(lstrArgumentList))
								{
									AfxMessageBox(IDS_CMDLINE_ERROR3);
									MSG_Mail(m_pBmContext);
								}
								else
								{   //command line is not in a recognizable format for this flag
									AfxMessageBox(IDS_CMDLINE_ERROR3);
									MSG_Mail(m_pBmContext);
									//ERROR!!!
								}
							}
						}
					}//End -COMPOSE CASE                 
					break;

				case STARTUP_ADDRESS:
					CAddrFrame::Open();
					break;

	       case STARTUP_IMPORT:
		   ImportExportLDIF(m_pABook, m_CmdLineLoadURL, iStartupMode);
		   return FALSE;

	       case STARTUP_EXPORT:
		   ImportExportLDIF (m_pABook, m_CmdLineLoadURL, iStartupMode);
		   return FALSE;
		   

				case STARTUP_NEWS:
					WFE_MSGOpenNews();      //starts the news and opens the default news server and group in split pain.
					//tell the browser to open commandline URL even though NEWS is set in preferences
					if(m_CmdLineLoadURL != NULL)
						iStartupMode |= STARTUP_BROWSER;
					break;


				case STARTUP_MAIL_NEWS:
					//We get here because of preferences and nothing but a file or no file was given on the command
					//line.
					WFE_MSGOpenInbox();     //opens the inbox folder with the split pain view.
					WFE_MSGOpenNews();              //opens the collections window
					//tell the browser to open commandline URL even though MAIL & NEWS are  set in preferences
					if(m_CmdLineLoadURL != NULL)
						iStartupMode |= STARTUP_BROWSER;
					break;

				case STARTUP_MAIL_NEWS_BROWSER:
			    //all three check boxes were set in preferences
					WFE_MSGOpenInbox();     //opens the inbox folder with the split pain view.
					WFE_MSGOpenNews();              //opens the collections window
					//browser will get launched in the code below.
					break;

				case STARTUP_MAIL_BROWSER:
					//mail and browser check boxes were checked
			    WFE_MSGOpenInbox(); //starts the inbox window.
					//browser will get launched in the code below
					break;

				case STARTUP_NEWS_BROWSER:
					//browser and news check boxes were checked
			    WFE_MSGOpenNews();  //opens the collections window
				    //browser will get launched in the code below.
					break;

				case STARTUP_BROWSER_EDITOR:
					//Code will get called below. for both browser and editor
					break;

				case STARTUP_MAIL_EDITOR:
					//mail and editor check boxes were checked
			    WFE_MSGOpenInbox(); //starts the inbox window.
					//browser will get launched in the code below
					//editor is launched in code below
					break;

			case STARTUP_NEWS_EDITOR:
			    WFE_MSGOpenNews();  //opens the collections window
				    break;              //editor will get launched in the code below.
							    //editor is launched in code below                  
						/*
			case STARTUP_NETCASTER:
			    FEU_OpenNetcaster() ;
						break;
						*/

			case STARTUP_MAIL_NEWS_EDITOR:
					WFE_MSGOpenInbox();     //opens the inbox folder with the split pain view.
					WFE_MSGOpenNews();      //opens the collections window
					//tell the browser to open URL even though it wasn't set in preferences
					if(m_CmdLineLoadURL != NULL)
						iStartupMode |= STARTUP_BROWSER;
					break;

			case STARTUP_MAIL_BROWSER_EDITOR:
			    WFE_MSGOpenInbox(); //starts the inbox window.
					 break;             //browser will get launched in the code below
					
			case STARTUP_NEWS_BROWSER_EDITOR:
			    WFE_MSGOpenNews();  //opens the collections window
					break;              //The rest is started below
					
			case STARTUP_MAIL_NEWS_BROWSER_EDITOR:
					WFE_MSGOpenInbox();     //opens the inbox folder with the split pain view.
					WFE_MSGOpenNews();      //opens the collections window
					break;              //tell the browser to open commandline URL even though MAIL & NEWS are      set in preferences
				
#endif /* MOZ_MAIL_NEWS */

#if defined(OJI) || defined(JAVA)
		case STARTUP_JAVA_DEBUG_AGENT:
					WFE_LJ_StartDebugger();         // stub call because java.h can't be included here
					break;
#endif  
				//default:
			}
		}

		//fall through here and launch browser if necessary
#ifdef MOZ_MAIL_NEWS
	if ((iStartupMode & STARTUP_BROWSER || iStartupMode & STARTUP_EDITOR || // if startup browser or editor
	    !(iStartupMode & (STARTUP_BROWSER|STARTUP_CALENDAR|STARTUP_MAIL|STARTUP_ADDRESS
			|STARTUP_INBOX|STARTUP_COMPOSE|STARTUP_FOLDER|STARTUP_FOLDERS|STARTUP_NETCASTER
      |STARTUP_CLIENT_MAPI|STARTUP_CLIENT_ABAPI)) ||    // or invalid data
	    m_bKioskMode ))  {                                                  // or kiosk mode - start browser
#else
	if ((iStartupMode & STARTUP_BROWSER || iStartupMode & STARTUP_EDITOR || // if startup browser or editor
	    !(iStartupMode & (STARTUP_BROWSER|STARTUP_NEWS|STARTUP_MAIL|STARTUP_ADDRESS
			|STARTUP_INBOX|STARTUP_COMPOSE|STARTUP_FOLDER|STARTUP_FOLDERS|STARTUP_NETCASTER|STARTUP_CALENDAR)) ||    // or invalid data
	    m_bKioskMode ))  {                                                  // or kiosk mode - start browser
#endif /* MOZ_MAIL_NEWS */

#ifdef EDITOR
	    if ( (bIsGold && (iStartupMode & STARTUP_EDITOR)) && !(iStartupMode & STARTUP_BROWSER))
			{   //start the editor
		theApp.m_EditTmplate->OpenDocumentFile(NULL);
				CMainFrame *pMainFrame = (CMainFrame *)FEU_GetLastActiveFrame(MWContextBrowser, TRUE);
				if(pMainFrame) pMainFrame->OnLoadHomePage();//suppose to load what ever was on the command line
	    }
			else
#endif // EDITOR
			if ((iStartupMode & STARTUP_BROWSER) && !(iStartupMode & STARTUP_EDITOR) )
			{       //start the browser
		    theApp.m_ViewTmplate->OpenDocumentFile(NULL);
					CMainFrame *pMainFrame = (CMainFrame *)FEU_GetLastActiveFrame(MWContextBrowser, FALSE);
				if(pMainFrame) pMainFrame->OnLoadHomePage(); //suppose to load what ever was on the command line
	    }
#ifdef EDITOR
			else if ((iStartupMode & STARTUP_BROWSER) && (iStartupMode & STARTUP_EDITOR) )
			{
				    //start both of these guys since there preferences were set 
				//theApp.m_EditTmplate->OpenDocumentFile(NULL);                             
		    theApp.m_ViewTmplate->OpenDocumentFile(NULL);  //open browser window
					CGenericFrame *pFrame = (CGenericFrame *)FEU_GetLastActiveFrame(MWContextBrowser, FALSE);
					if (pFrame){
						CMainFrame *pMainFrame = (CMainFrame*)pFrame;
						pMainFrame->OnLoadHomePage();
						pFrame->OnOpenComposerWindow();
					}
			}
#endif // EDITOR
	}

		if(m_pFrameList && m_pFrameList->GetMainContext())
		{
	    NET_CheckForTimeBomb(m_pFrameList->GetMainContext()->GetContext());
		}

	}

    //--------------------------------------------------------------------
    //                           IMPORTANT
    //                           =========
    // The user's homepage could have already been loaded.  Any init code
    //   below this line better not be needed for page loading
    //--------------------------------------------------------------------

#ifdef MOZ_TASKBAR
	// Start the Task Bar up - this must be done AFTER all frame windows
	// are created. Get the state flags and last saved position from
	// the registry.
	ASSERT(!m_TaskBarMgr.IsInitialized());
	m_TaskBarMgr.LoadPrefs(m_bAlwaysDockTaskBar);
#endif /* MOZ_TASKBAR */  

    //  DDE is hosed on Windows95 October 94 release.  Allow the user to turn it off
	//      This should be the very last thing done in initinstance, always.
    if(GetProfileString("Main", "DDE Hosed", "no") == "no")     {
	extern void DDEStartup(void);
	DDEStartup();
	}

	// read in the page setup information
    LoadPageSetupOptions();

    //  Start any timeout events.
    FEU_GlobalHistoryTimeout((void *)1);

    //  Leave this as the very last thing to do.
    //  If we've a print command to exectute, do it now.
    if(csPrintCommand.IsEmpty() == FALSE)   {
	OnDDECommand((char *)(const char *)("/print" + csPrintCommand));
    }

    //  Only do the below when we think we're really ready to exit InitInstance successfully.
    //  Previously, these were way up towards the top of initinstance, but have been
    //      put here as they cause wild effects with Inserting an OLE object while
    //      not currently running under win32.

	    //  Connect the COleTemplateServer to the view document template.
	    //  The COleTemplateServer creates new documents on the behalf of
	    //          requestion OLE containers by using information specified in the
	    //          document template.
	    m_OleTemplateServer.ConnectTemplate(clsid, pTemplate, FALSE);

	    //  Register all OLE server (factories) as running.
	//  Also set up for shell open through DDE
	    //  This enables the OLE libraries to create objects from other apps.
	    //  Do not change the calling order here, please, or you may break OLE.
	COleTemplateServer::RegisterAll();
	EnableShellOpen();
	RegisterShellFileTypes();

	//  Always set that we are an in place server.
	COleObjectFactory::UpdateRegistryAll();
	//  This get's in the way of debugging under nt351 with service pack 4 installed, just don't do it....
	m_OleTemplateServer.UpdateRegistry(OAT_INPLACE_SERVER);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Cleanup our toys and go home

int CNetscapeApp::ExitInstance()
{
	//	When receiving a WM_ENDSESSION, ExitInstance may be entered twice.
	//	Block the code on the second or more attempts.
	//	The exact nature happens because the OnIdle code in the app detects
	//		a lack of all contexts and sends a redundant close message to
	//      the application's hidden frame, even though this is already
	//		done by the hidden frame (m_pMainWnd) when it receives the 
	//      WM_ENDSESSION message.

	static int iExitCounter = -1;
	iExitCounter++;
	if(iExitCounter != 0)   {
		TRACE("Blocking %d reduntant call(s) to CNetscapeApp::ExitInstance.\n", iExitCounter);
	    return(0);
	}
	
	//unregister our DDE service and free the ServiceName Handle
	if(CDDECMDWrapper::m_dwidInst)
	{
		DdeNameService(CDDECMDWrapper::m_dwidInst,
			CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ServiceName], NULL,
			DNS_UNREGISTER);
		DdeFreeStringHandle(CDDECMDWrapper::m_dwidInst,
			CDDECMDWrapper::m_hsz[CDDECMDWrapper::m_ServiceName]);
	}


	//m_bExitStatus - Used to detect exit status from second instance.
	m_bExitStatus = TRUE;

	//  Tell the slave window that we are exiting.
	//  Anyone observing the event will deregister themselves.
	slavewnd.WindowProc(SLAVE_EXITINSTANCE, 0, 0);

	TRACE("CNetscapeApp::ExitInstance entered\n");

	if (!m_strStubFile.IsEmpty())
		remove(m_strStubFile);

#ifdef MOZ_TASKBAR
	// Save Task Bar state
	m_TaskBarMgr.SavePrefs();
#endif /* MOZ_TASKBAR */

	WFE_DestroyUIPalette();
	WFE_DestroyBitmapMap();
	WFE_DestroyUIFont();

#ifdef EDITOR
    WFE_ExitComposer();
#endif // EDITOR

#ifdef XP_WIN32
#ifdef EDITOR
    // Tell Site Manager we are going away
    if ( pITalkSMClient != NULL ) {
	delete ( pITalkSMClient );
#if 0
	TALK_UnregisterApp();
#endif
    }
#endif // EDITOR
#endif // XP_WIN32

	// Shutdown RDF
	RDF_Shutdown();

	SHUTDOWN_np();

    //  Unload any remaining images.
    IL_Shutdown();

    // shut down exchange if enabled
    FEU_CloseMapiLibrary();

    //  Write out our list of trusted applications.
    if( m_pSpawn ) {
	delete m_pSpawn;
    }

    GH_SaveGlobalHistory();
    GH_FreeGlobalHistory();

#ifdef MOZ_MAIL_NEWS
    // write out newsrc stuff
    NET_SaveNewsrcFileMappings();
#endif /* MOZ_MAIL_NEWS */

    // cleanup intenal file type stuff
    fe_CleanupFileFormatTypes();

    PREF_SavePrefFile();
    NR_ShutdownRegistry();
#ifdef MOZ_SMARTUPDATE
    SU_Shutdown();
#endif

    //  Save certs and keys early, since if they are lost the user is screwed
    SECNAV_Shutdown();

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    //  Shut down mocha.
    ET_FinishMocha();
#endif /* MOZ_NGLAYOUT */


    BOOL javaShutdownSuccessful = fe_ShutdownJava();

    if(!m_pCacheDir.IsEmpty())
	NET_CleanupCacheDirectory((char *)(const char *)m_pCacheDir, CACHE_PREFIX);
    NET_ShutdownNetLib();

#ifdef MOZ_MAIL_NEWS
#ifndef MOZ_LITE
    if (m_pABook)
		AB_CloseAddressBook(&m_pABook);
#endif
    WFE_MSGShutdown();
#endif /* MOZ_MAIL_NEWS */

#ifdef MOZ_LDAP
    DIR_DeleteServerList(m_directories);
#endif /* MOZ_LDAP */

    PREF_Cleanup();


    POSITION pos;
    CString key;
    CDNSObj * obj;

    // Iterate through the entire map
    for( pos = DNSCacheMap.GetStartPosition(); pos != NULL; )
    {
	DNSCacheMap.GetNextAssoc( pos, key, (CObject *&)obj );
	if (obj) delete obj;
    }

    // delete cached memory CDCs
    if(pIconDC)
	delete pIconDC;
    if(pImageDC)
	delete pImageDC;

    // save the page setup options
    SavePageSetupOptions();

    // Resource cleanup
    if(m_CmdLineLoadURL)
	XP_FREE(m_CmdLineLoadURL);

    // Delete any temp files we have created
    FE_DeleteTempFilesNow();


    // Unload PE MUC dll
    if ((UINT)m_hPEInst > 32)
	FreeLibrary(m_hPEInst);

	// Unload resource dll
    if ((UINT)m_hResInst > 32)
	FreeLibrary(m_hResInst);
    // Free memory used for WideAPI(Unicode) conversion
#ifdef XP_WIN32
    if (theApp.m_bUseUnicodeFont && CIntlWin::m_wConvBuf)
	free(CIntlWin::m_wConvBuf);
#else
    Ctl3dUnregister(m_hInstance);
#endif

#ifndef MOZ_NGLAYOUT
	SHUTDOWN_cvffc();
#endif /* MOZ_NGLAYOUT */
    //  Free off various allocated memory.
    if(XP_AppName)    {
	XP_FREE(XP_AppName);
	XP_AppName = NULL;
    }
    if(XP_AppCodeName)    {
	XP_FREE(XP_AppCodeName);
	XP_AppCodeName = NULL;
    }
    if(XP_AppVersion)    {
	XP_FREE(XP_AppVersion);
	XP_AppVersion = NULL;
    }
    if(XP_AppLanguage)    {
	XP_FREE(XP_AppLanguage);
	XP_AppLanguage = NULL;
    }

    //  Finally, do our OLE shutdown notifications.
    COleRegistry::Shutdown();

    // DDE is hosed on Windows95 October 94 release.  Allow the user to turn it off.
    //   If we never turned it on don't turn it off now.
    if(GetProfileString("Main", "DDE Hosed", "no") == "no")     {
	extern void DDEShutdown(void);
	DDEShutdown();
    }

    WSACleanup();

	// clean up the font cache here.
	XP_List *theList = m_appFontList;
	NsWinFont* theFont;
	int count = XP_ListCount(theList);
	for (int i = 0; i < count; i++) {
		theFont = (NsWinFont*)XP_ListRemoveTopObject (theList);
		::DeleteObject(theFont->hFont);
		XP_DELETE( theFont);
	}
	XP_ListDestroy(theList);

	// Delete our custom images in NavCenter (Dave H.)
	CHTFEData::FlushIconInfo();

    //
    // Free all resources allocated by NSPR...
    //
#if !defined(_WIN32)
#if defined(OJI) || defined(JAVA) || defined(MOCHA)
#if defined(NSPR20)
	/*
	 * XXX SHould PR_CLeanup be called here?
	 *
	 * In nspr10 PR_Shutdown unloads all the dlls
	 * In nspr20 PR_shutdown is equivalent to the socket shutdown call
	 *	  and implict cleanup unloads the dlls for win16
	 */	
#else
    PR_Shutdown();
#endif
#endif
#endif  /* OJI || JAVA || MOCHA */

	if (theApp.m_bNetworkProfile) {
		// if we have a network profile command-line we are assuming delete on exit (for now)
		CString pTempDir;
		int ret;
		XP_StatStruct statinfo; 


        if(getenv("TEMP"))
          pTempDir = getenv("TEMP");  // environmental variable
        if (pTempDir.IsEmpty() && getenv("TMP"))
          pTempDir = getenv("TMP");  // How about this environmental variable?
		if (pTempDir.IsEmpty()) {
			  // Last resort: put it in the windows directory
		  char path[_MAX_PATH];
		  GetWindowsDirectory(path, sizeof(path));
		  pTempDir = path;
		}

		pTempDir += "\\nstmpusr";		
		ret = _stat((char *)(const char *)pTempDir, &statinfo);
		if (ret == 0) {
			XP_RemoveDirectoryRecursive(pTempDir, xpURL);
		}
	}

    TRACE("CNetscapeApp::ExitInstance leaving\n");

#if defined( _DEBUG) && defined( XP_WIN32 )
	if (m_pi.hProcess) {
		TerminateProcess(m_pi.hProcess, 0);
		CloseHandle(m_pi.hProcess);
	}
#endif

    return(CWinApp::ExitInstance());
}
