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

// mozilla.h : main header file for the NETSCAPE application
//

#ifndef _NETSCAPE_H
#define _NETSCAPE_H

#ifdef MOZ_MAIL_NEWS  // Is this the right ifdef to use
#include "csttlbr2.h"
#endif /* MOZ_MAIL_NEWS */

#ifdef MOZ_TASKBAR
#include "taskbar.h"
#endif /* MOZ_TASKBAR */

// This may have to be enabled
#if 0
#include "widgetry.h"
#endif 


#include "splash.h"
#include "intlwin.h"
#include "woohoo.h"
#include "nsfont.h"
#include "postal.h" 
#include "libmime.h"
#include "rdf.h"
#include "rdfacc.h"

#include "ownedlst.h"
#ifdef XP_WIN32
#include "mmsystem.h"
#endif

extern int winfeInProcessNet;

// navigator startup states
#define STARTUP_BROWSER     0x1
#define STARTUP_MAIL        0x2
#define STARTUP_NEWS        0x4
#define STARTUP_EDITOR      0x8

#define STARTUP_MAIL_NEWS                 (STARTUP_MAIL | STARTUP_NEWS)
#define STARTUP_MAIL_BROWSER          (STARTUP_MAIL | STARTUP_BROWSER)
#define STARTUP_NEWS_BROWSER          (STARTUP_NEWS | STARTUP_BROWSER)
#define STARTUP_MAIL_NEWS_BROWSER     (STARTUP_MAIL | STARTUP_NEWS | STARTUP_BROWSER)
#define STARTUP_MAIL_EDITOR                   (STARTUP_MAIL | STARTUP_EDITOR)
#define STARTUP_NEWS_EDITOR                   (STARTUP_NEWS | STARTUP_EDITOR)
#define STARTUP_BROWSER_EDITOR            (STARTUP_BROWSER | STARTUP_EDITOR)
#define STARTUP_MAIL_NEWS_EDITOR      (STARTUP_MAIL | STARTUP_NEWS | STARTUP_EDITOR)
#define STARTUP_MAIL_BROWSER_EDITOR   (STARTUP_MAIL | STARTUP_BROWSER | STARTUP_EDITOR)
#define STARTUP_NEWS_BROWSER_EDITOR   (STARTUP_NEWS | STARTUP_BROWSER | STARTUP_EDITOR)
#define STARTUP_MAIL_NEWS_BROWSER_EDITOR     (STARTUP_MAIL | STARTUP_NEWS | STARTUP_BROWSER | STARTUP_EDITOR)
// component launch states
#define STARTUP_INBOX           0x10
#define STARTUP_FOLDER          0x20    
#define STARTUP_CLIENT_MAPI      0x30  // rhp - DOES THIS WORK - will it break other startups???
#define STARTUP_FOLDERS         0x40
#define STARTUP_ADDRESS         0x80
#define STARTUP_COMPOSE         0x100
#define STARTUP_IMPORT          0x200
#define STARTUP_EXPORT          0x300
#define STARTUP_NETCASTER       0x400
#define STARTUP_ACCOUNT_SETUP   0x800
#define STARTUP_JAVA_DEBUG_AGENT 0x1000
#define STARTUP_JAVA			0x1200
#define STARTUP_CALENDAR        0x4000
#define STARTUP_CONFERENCE      0x8000
#define STARTUP_NETHELP         0x2000

// max list of things in the menu
#define MAX_HISTORY_ITEMS   (LAST_HISTORY_MENU_ID-FIRST_HISTORY_MENU_ID)
#define MAX_SECURITY_CHECKS 12
#define MAX_BOOKMARK_ITEMS  (LAST_BOOKMARK_MENU_ID-FIRST_BOOKMARK_MENU_ID)
#define MAX_FILE_BOOKMARK_ITEMS (LAST_FILE_BOOKMARK_MENU_ID-FIRST_FILE_BOOKMARK_MENU_ID)

#define MAX_INTERNAL_OLEFORMAT 4

#define FIRST_SHORTCUT_ID               (IDC_SHORTCUT_BASE)   // 1800
#define LAST_SHORTCUT_ID                (FIRST_SHORTCUT_ID + 50)
#define FIRST_PLACES_MENU_ID    (IDC_FIRST_PLACES_MENU_ID)
#define LAST_PLACES_MENU_ID             (FIRST_PLACES_MENU_ID + 50)
#define FIRST_HELP_MENU_ID              (IDC_FIRST_HELP_MENU_ID)
#define LAST_HELP_MENU_ID               (FIRST_HELP_MENU_ID + 50)

#define IS_BOOKMARK_ID(nCmdId)\
	((nCmdId) >= FIRST_BOOKMARK_MENU_ID && (nCmdId) <= LAST_BOOKMARK_MENU_ID)

#define IS_HISTORY_ID(nCmdId)\
	((nCmdId) >= FIRST_HISTORY_MENU_ID && (nCmdId) <= LAST_HISTORY_MENU_ID)

#define IS_FILE_BOOKMARK_ID(nCmdId)\
	((nCmdId) >= FIRST_FILE_BOOKMARK_MENU_ID && (nCmdId) <= LAST_FILE_BOOKMARK_MENU_ID)

#define IS_SHORTCUT_ID(nCmdId) \
	((nCmdId) >= FIRST_SHORTCUT_ID && (nCmdId) <= LAST_SHORTCUT_ID)

#define IS_PLACESMENU_ID(nCmdId) \
	((nCmdId) >= FIRST_PLACES_MENU_ID && (nCmdId) <= LAST_PLACES_MENU_ID)

#define IS_HELPMENU_ID(nCmdId) \
	((nCmdId) >= FIRST_HELP_MENU_ID && (nCmdId) <= LAST_HELP_MENU_ID)
 
// A FE specific structure to store info we need about images

#ifdef LAYERS
// Event data for compositor-based event dispatch
// Is this the right place for this??
typedef struct _fe_EventStruct {
    UINT uFlags;
    UINT nChar;
    int32 x, y;
    BOOL pbReturnImmediately;
    LO_Element *pElement;    
    uint32 fe_modifiers;
} fe_EventStruct;

#endif /* LAYERS */

#define IMAGE_ICON_SIZE         32
#define NEWS_X_SIZE                 88
#define NEWS_Y_SIZE         36
#define GOPHER_ICON_SIZE    20


// How to handle an external application
#define HANDLE_VIA_NETSCAPE  50
#define HANDLE_UNKNOWN      100
#define HANDLE_EXTERNAL     200
#define HANDLE_SAVE                 300
#define HANDLE_VIA_PLUGIN   400
#define HANDLE_MOREINFO     500
#define HANDLE_SHELLEXECUTE 600
#define HANDLE_BY_OLE           700

#define TB_PICTURESANDTEXT 0
#define TB_PICTURES 1
#define TB_TEXT 2

// Limits the number of publish and template history URLs to save in preferences
#define MAX_PUBLISH_LOCATIONS  20
#define MAX_TEMPLATE_LOCATIONS 20

// This flag allows differences between Gold and Non-gold 
//   behavior in code common to both (e.g., SiteManager interaction)
extern BOOL bIsGold;

#ifdef XP_WIN32
// System for communication between Navigator/Editor and Site Manager
#include "talk.h"
#ifdef EDITOR
extern ITalkSMClient * pITalkSMClient;
#endif /* EDITOR */
// This should be set ONLY if pITalkSMClient is not null
extern BOOL bSiteMgrIsRegistered;
// This is set if we find an existing window on startup
//   or we invoke site manager from menu
extern BOOL bSiteMgrIsActive;

// Message ID to find out if SiteManager is active,
//   or for it to tell us it has been activated or is closing down
extern "C" UINT WM_SITE_MANAGER;
#endif

// These are obtained by RegisterWindowMessage() when app starts
// Used to detect 1st instance and start Editor or Navigator
//   when 2nd instance is attempted
extern "C" UINT WM_NG_IS_ACTIVE;
extern "C" UINT WM_OPEN_EDITOR;
extern "C" UINT WM_OPEN_NAVIGATOR;

extern char szNGMemoryMapFilename[];

//
// Function pointers for keeping track of the MAPI DLL if we have
//   loaded it.  
//
typedef BOOL (WINAPI* REGISTERMAIL)(HWND, const char *);
typedef BOOL (WINAPI* OPENMAIL)(const char *, const char *);
typedef BOOL (WINAPI* COMPOSEMAIL)(const char *, const char *,
    const char *, const char *, const char *, const char *, const char *, const char*); 
typedef BOOL (WINAPI* UNREGISTERMAIL)(void);
typedef BOOL (WINAPI* SHOWMAILBOX)(void); 
typedef BOOL (WINAPI* SHOWMESSAGECENTER)(void); 
typedef BOOL (WINAPI* CLOSEMAIL)(void); 
typedef BOOL (WINAPI* GETMENUITEMSTRING)(ALTMAIL_MENU_ID, char *, int);

// msgcom.h types
//
class MSG_Prefs;

// AddressBook types
//
class ABook;
class CCmdParse;

/////////////////////////////////////////////////////////////////////////////
// CNetscapeApp:
// See mozilla.cpp for the implementation of this class
//

// this is for us to take care for font caching.
struct NsWinFont 
{
   LOGFONT logFontStruct;
   HFONT   hFont;
   short   refCount;
};

class CNetscapeApp : public CWinApp
{
 //  Wether or not we allow UNC file names.
 public:
     BOOL m_bUNC;

//  Wether or not we're in init instance, and the cached value of m_nCmdShow.
public:
    BOOL m_bInInitInstance;
	BOOL m_bInGetCriticalFiles;
    int m_iFrameCmdShow;

//      OLE Server template.
public:
	COleTemplateServer m_OleTemplateServer;
//  Shell extension registrations.
private:
    void EnableShellStuff();
	
public:
    CNetscapeApp();
    int ExitInstance();
    BOOL m_bExit;
	BOOL m_bExitStatus;
    CNetscapeDocTemplate  *m_ViewTmplate;
#ifdef EDITOR
    // We now create the EditTemplate class
    //  even in non-editors, but we do NOT
    //  allocate menu and accelerators
    //  if we are not an editor
    CNetscapeEditTemplate *m_EditTmplate;

    CNetscapeComposeTemplate *m_ComposeTemplate;
    CNetscapeTextComposeTemplate *m_TextComposeTemplate;
    CNetscapeAddrTemplate *m_AddrTemplate;
#endif // EDITOR

    CWnd * m_pUpdateWhatsNewWnd;
    MWContext * m_pBmContext;
#ifdef MOZ_MAIL_NEWS
    MWContext * m_pAddressContext;
#endif
    CStubsCX *m_pSlaveCX;
    CRDFCX *m_pRDFCX;
    HANDLE hAccelTable;
    DWORD m_dwSplashTime;
    BOOL m_bDontLoadHome;
    HWND m_ParentAppWindow;
    BOOL m_bChildWindow;

    //  Our very own run/pump/idle implementation.
public:
    int Run();
    BOOL NSPumpMessage();
#ifdef MOZ_LOC_INDEP
    /* location independence exit code */
    BOOL LIStuffEnd();
#endif /* MOZ_LOC_INDEP */
    BOOL IsIdleMessage(MSG *pMsg);
    BOOL OnIdle(LONG lIdleCount);
#ifdef XP_WIN16
    CPoint m_ptCursorLast;      // last mouse position
    UINT m_nMsgLast;            // last mouse message
#endif


    CString NSToolBarClass;
    //  Trusted application spawn list.
    CSpawnList *m_pSpawn;

	// List of owned and lost file types (for the default browser check)
	COwnedAndLostList m_OwnedAndLostList;

    //      The hidden frame.
    CHiddenFrame *m_pHiddenFrame;

    //      The list of top level frames.
    CGenericFrame   *m_pFrameList;
#ifdef MOZ_MAIL_NEWS
    CAddrFrame          *m_pAddressWindow;
    ABook               *m_pABook;
    XP_List             *m_directories;
#endif /* MOZ_MAIL_NEWS */

public:
    time_t GetTime()    {
	DWORD dwElapsedTicks = m_dwMsgTick - m_dwStartTick;
	DWORD dwElapsedSeconds = dwElapsedTicks / 1000;
	return(m_ttStartTime + dwElapsedSeconds);
    }
    void InitTime();
    DWORD GetMessageTime()  {
	return(m_dwLastMsgTick);
    }
	
	HFONT CreateAppFont(LOGFONT& logFont);
	void ReleaseAppFont(HFONT logFont);

//------------
//	Dave H. - The following functions are for NavCenter.
	CNSNavFrame* CreateNewNavCenter(CNSGenFrame* pParentFrame=NULL, BOOL useViewType = FALSE, HT_ViewType viewType = HT_VIEW_BOOKMARK);
//--- End Nav Center.

private:
    time_t m_ttStartTime;
    DWORD m_dwStartTick;
    DWORD m_dwMsgTick;
    DWORD m_dwLastMsgTick;
    XP_List* m_appFontList;

#if defined( _DEBUG) && defined( XP_WIN32 )
    // Debug Trace window
    PROCESS_INFORMATION 	m_pi;
    int InitConsoleWindow(void);
#endif

private:
#ifdef MOZ_TASKBAR
    CTaskBarMgr m_TaskBarMgr;
#endif /* MOZ_TASKBAR */
	
#ifdef XP_WIN16
    enum HELP_IDLE_TYPE{SENT_MESSAGE=1,NOT_SENT=2};
    HELP_IDLE_TYPE m_helpstate;
#endif

public:
#ifdef MOZ_TASKBAR
    CTaskBarMgr & GetTaskBarMgr()
	{
		return(m_TaskBarMgr);
	}
#endif /* MOZ_TASKBAR */
	
    //  Explicitly use idle binding to timeout code if no timers available.
    BOOL m_bIdleProcessTimeouts;

    //  Are we the default browser (as opposed to the others trouncing all over our
    //      registry entries)?
	void MakeDefaultBrowser();
    void CheckDefaultBrowser();


    ////////////////////////////////////////////////////////
    // Cache two memory CDCs for faster drawing
    CDC * pIconDC;     // drawing stuff NOT in main view
    CDC * pImageDC;    // drawing stuff in the main view area

    // override INI functions so that we goto the Registry in Win32
    UINT    GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault);
    CString GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL);
    BOOL    WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue);
    BOOL    WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue);
    CString m_pSARCacheDir;                 /* larubbio */
    DWORD   GetPrivateProfileString(LPCSTR lpSectionName, LPCSTR lpKeyName, LPCSTR lpDefault, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName);

    ///////////////////////////////////////////////////////////////////////
    // Does this binary use a locked preference file?
    BOOL    m_bUseLockedPrefs;
    CString m_csLockedPrefs;

    ///////////////////////////////////////////////////////////////////////
    // MAPI function pointers and state information
    HINSTANCE       m_hPostalLib;
    OPENMAIL        m_fnOpenMailSession;
    COMPOSEMAIL     m_fnComposeMailMessage;
    UNREGISTERMAIL  m_fnUnRegisterMailClient;
    SHOWMAILBOX       m_fnShowMailBox; 
    SHOWMESSAGECENTER m_fnShowMessageCenter; 
    CLOSEMAIL         m_fnCloseMailSession; 
    GETMENUITEMSTRING m_fnGetMenuItemString; 
    BOOL              m_bInitMapi;

    ///////////////////////////////////////////////////////////////////////
    // The main User Directory pointer
    CString m_UserDirectory;

    int     m_iTemplateLocationCount;

    ///////////////////////////////////////////////////////////////////////
    // Application level preferences
    CString m_pCacheDir;
    char * m_pTempDir;

    CString m_pTelnet;
    CString m_pTn3270;
    CString m_pHTML;

    // bookmark file location
    CString m_pBookmarkFile;

    // Appearance of toolbars/component bars
    int m_pToolbarStyle;

    // preference type stuff
    int    m_nConfig;
    int    m_iNumTypesInINIFile;
    CString m_pHomePage;
    int             m_iCSID;        // Default encoding

    // Should telnet urls be parsed into host/port?
    BOOL m_bParseTelnetURLs;

    // dialup stuff
    BOOL m_bKioskMode;
    BOOL m_bSuperKioskMode;
    BOOL m_bNetworkProfile;
#ifdef MOZ_MAIL_NEWS
    BOOL m_bCreateNews;
    BOOL m_bCreateMail;
#endif /* MOZ_MAIL_NEWS */
    BOOL m_bCreateNetcaster;
#ifdef EDITOR
    // "-EDIT" or "/EDIT" on command line causes us to start editor
    BOOL m_bCmdEdit;
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS
	//component launch flags
	BOOL m_bCreateInbox;    //causes the inbox to be started
  BOOL m_bCreateInboxMAPI;   // start inbox minimized!
	BOOL m_bCreateFolders;  //causes the folders frame window to be started
	BOOL m_bCreateFolder;   //causes a particular folder in the folders frame window to open
	BOOL m_bCreateCompose;  //causes the compose window to be opened, 3 different possibilities
#endif /* MOZ_MAIL_NEWS */
#ifdef EDITOR
	BOOL m_bCreateEdit;             //brings up the edit window
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS
	BOOL m_bCreateAddress;  //openes the address book window
#endif // MOZ_MAIL_NEWS
	BOOL m_bCreateLDIF_IMPORT; //imports an LDIF file
    BOOL m_bCreateLDIF_EXPORT; //exports an LDIF file
	BOOL m_bCreateBrowser;  //forces the browser to be launch despite preferences settings
	BOOL m_bCreateJavaDebugAgent;    //causes the java debug agent to be started
	BOOL m_bHasArguments;   //tells us if we have more work to do with the command line
							//if after removing any preceding switch, there are still parameters
							//on the command line this var is set to TRUE
	BOOL m_bAlwaysDockTaskBar;  //tells us to dock the taskbar regardless of preferences

	// "-new_profile" on command line causes us to create a new profile
	BOOL m_bCreateNewProfile;
	// "-profile_manager" on command line causes us to launch profile manager
	BOOL m_bProfileManager;

	// security checks      
	Bool   m_nSecurityCheck[MAX_SECURITY_CHECKS];

	// Use unicode font on NT ?
    BOOL    m_bUseUnicodeFont;
    BOOL    m_bUseVirtualFont;

	// Should the browser window show the Netscape Button
	BOOL	m_bShowNetscapeButton;
	// End of preferences
	///////////////////////////////////////////////////////////////////////////
	
	char        * m_CmdLineLoadURL;
	char            * m_CmdLineProfile;
    CWnd        * m_pBookmarks;
#ifdef MOZ_MAIL_NEWS
    CWnd        * m_pAddressBook;
#endif /* MOZ_MAIL_NEWS */
	CSplashWnd  m_splash;  

 	MWContext	* m_pNetcasterWindow;		

	
#ifdef FORTEZZA
	CBitmap  * m_pFortezzaSecureBmp;
#endif

	CMapStringToObNoCase  m_HelperListByType;

    // Message ID for the Event notifications sent by NSPR
    UINT m_msgNSPREventNotify;

    // parameters of the last find operation so that Find Again will work correctly
    // These should probably be stuck in a struct or something for encapsulation
    CString m_csFindString;
    CString m_csReplaceString;
	CString m_strStubFile;//used for launching compose from command line with only an attachment given.
    BOOL    m_bMatchCase;
    BOOL    m_bSearchDown;

	CIntlFont *m_pIntlFont;
	HINSTANCE m_hResInst;  // Instance handle for resource dll

// Overrides
    virtual BOOL InitApplication();
    virtual BOOL InitInstance();
	virtual BOOL OnDDECommand(char *pszCommand);
	int MessageBox(HWND hWnd, LPCSTR lpszText,
		       LPCSTR lpszCaption = NULL,
		       UINT nType = MB_OK);

// Implementation
	BOOL m_bEmbedded;       //      Wether or not running embedded
	BOOL m_bAutomated;      //      Wether or not running automated

    int m_iCmdLnX;      //      X position on the command line.
	int m_iCmdLnCX;
	int m_iCmdLnY;  //      Y position on the command line.
	int m_iCmdLnCY;

	// OEM stuff                     
	int m_nChangeHomePage;  // some OEMs want user to be unable to change homepage
	int m_nNetscapeInToolbar;  // draw a little N in the toolbar for OEM people 
    // end of OEM stuff
	//component launch stuff  A.J.
	BOOL ParseComponentArguments(char *pszCommandLine, BOOL bRemove);
	BOOL ExistComponentArguments(char *pszCommandLine);
	void LaunchComponentWindow(int iStartupMode, char *pszCmdLine);
	void SetStartupMode(int *iStartupMode);
	void BuildStubFile();//Sets the m_strStubFile member
    CString BuildDDEFile();//Returns a CString type to a random file name


#ifdef MOZ_MAIL_NEWS
	BOOL LaunchComposeAndAttach(CStringList &rlstrArgumentList);
	MSG_AttachmentData * BuildAttachmentListFromFile(char *pszAttachFile);
#endif /* MOZ_MAIL_NEWS */
	BOOL BuildCmdLineList(const char* pszRemoveString,CCmdParse &cmdParse, CStringList &rlstrList, char* pszCmdLine);
    BOOL ImportExportLDIF(ABook *pBook, char *pszFileName, int action); 
    BOOL ProcessCommandLineDDE(char *pszDDECommand);
    void RemoveDDESyntaxAndSwitch(CString strSwitch, char *pszCmdLine);

	//end of component launch stuff

	void parseCommandLine(char * commandLine);
	int RuntimeIntSwitch(const char *pSwitch);
	CString RuntimeStringSwitch(const char *pSwitch, BOOL bCheckCommand = TRUE);
	BOOL IsRuntimeSwitch(const char *pSwitch,BOOL bRemove=TRUE);
	BOOL IsRuntimeSwitch(const char *pSwitch,char *pszCommandLine, BOOL bRemove=TRUE);

    BOOL LoadPageSetupOptions();
    void SavePageSetupOptions();

    // load the network and stuff
    void InitNetwork();

    //  Amazing security and app version information.
    CString m_csWinsock;    //  Description of running winsock.
    CString ResolveShortAppVersion();  //  Create first part of user/agent 
    CString ResolveAppVersion();    //  Create the agent part of the user/agent field.
	void StoreVersionInReg();

    // Re-Implement bogus CWnd::SendMessageToDescendants(...) method
    static void PASCAL SendMessageToDescendants(HWND hWnd, UINT message,
						WPARAM wParam, LPARAM lParam); 

	// Are we Personal Edition?
	BOOL      m_bPEEnabled;
	HINSTANCE m_hPEInst;
	BOOL      m_bAccountSetup;
	BOOL	  m_bAccountSetupStartupJava;
	CString   m_csPEPage;  // Don't use this if one-time homepage happens

    void CommonAppExit();

	inline BOOL showSplashScreen(const CString &csPrintCommand); 

    //{{AFX_MSG(CNetscapeApp)
    afx_msg void OnAppAbout();
	afx_msg void OnAppExit();
	afx_msg void OnAppSuperExit();
    afx_msg void OnHelp();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};


// Used to factor some code out for MOZ_MAIL_NEWS
BOOL CNetscapeApp::showSplashScreen(const CString &csPrintCommand)
{
#ifdef MOZ_MAIL_NEWS
	return (!(m_bEmbedded || m_bAutomated || m_bCreateInboxMAPI) && csPrintCommand.IsEmpty());
#else
	return (!(m_bEmbedded || m_bAutomated) && csPrintCommand.IsEmpty());
#endif
}

/////////////////////////////////////////////////////////////////////////////

extern CNetscapeApp NEAR theApp;

//#ifdef MOZ_NETSCAPE_FONT_MODULE
// the only one object of CNetscapeFontModule
extern CNetscapeFontModule    theGlobalNSFont;
//#endif // MOZ_NETSCAPE_FONT_MODULE 

// Home Grown resources
#define IDC_URL_EDIT     25000
#define IDC_HIST_LIST    25001
#define DYNAMIC_RESOURCE 25010

// Speed of animation 0 == fast 1000 == once per sec
#define ANIMATE_ICON_SPEED      300

// Bogus temp dir name so we can see if we've gotten one or not
#define BOGUS_TEMP_DIR                          "xxxxx"

// Win16 has _MAX_PATH, not MAX_PATH catch easy and frequent mistakes
#ifndef MAX_PATH
#define MAX_PATH _MAX_PATH
#endif

#endif

