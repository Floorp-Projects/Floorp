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

// mainfrm.cpp : implementation of the CMainFrame class
//                                           

#include "stdafx.h"

#include "dialog.h"																  
#include "mainfrm.h"
#include "netsvw.h"
#include "toolbar.cpp"
#include "usertlbr.h"
#include "urlbar.h"
#include "prefapi.h"
#include "csttlbr2.h"
#include "prefinfo.h"
#include "libevent.h"
#include "navfram.h"
#include "edview.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CMainFrame, CGenericFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

const UINT NEAR msg_RegistrationComplete = RegisterWindowMessage("NetscapeRegistrationComplete");
const UINT NEAR msg_AbortRegistration = RegisterWindowMessage("NetscapeAbortRegistration");

#define FILEMENU 0
#define EDITMENU 1
#define VIEWMENU 2
#define GOMENU   3

BEGIN_MESSAGE_MAP(CMainFrame, CGenericFrame)
    //{{AFX_MSG_MAP(CMainFrame)
    ON_WM_INITMENUPOPUP()
    ON_WM_MENUSELECT()
    ON_WM_SHOWWINDOW()
    ON_COMMAND(ID_OPTIONS_TITLELOCATION_BAR, OnOptionsTitlelocationBar)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_TITLELOCATION_BAR, OnUpdateOptionsTitlelocationBar)
    ON_WM_TIMER()
    ON_WM_WINDOWPOSCHANGED()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_COMMAND(ID_OPTIONS_TOGGLENETDEBUG, OnOptionsTogglenetdebug)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_LOADINLINEDIMAGES, OnUpdateToggleImageLoad)
    ON_COMMAND(ID_HOTLIST_MCOM, OnNetscapeHome)
	ON_COMMAND(ID_PLACES, OnGuide)
    ON_COMMAND(ID_OPTIONS_FLUSHCACHE, OnFlushCache)
    ON_COMMAND(ID_OPTIONS_SHOWFTPFILEINFORMATION, OnToggleFancyFtp)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWFTPFILEINFORMATION, OnUpdateToggleFancyFtp)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWSTARTERBUTTONS, OnUpdateOptionsShowstarterbuttons)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
    ON_WM_CLOSE()
    ON_WM_DROPFILES()
    ON_COMMAND(ID_HELP_SECURITY, OnHelpSecurity)
    ON_WM_SYSCOMMAND()
    ON_WM_SETFOCUS()
	ON_COMMAND(ID_VIEW_TOOLBAR, OnOptionsViewToolBar)
    ON_COMMAND(ID_OPTIONS_LOADINLINEDIMAGES, OnToggleImageLoad)
    ON_COMMAND(ID_OPTIONS_SHOWSTARTERBUTTONS, OnOptionsShowstarterbuttons)
	ON_COMMAND(ID_OPEN_MAIL_WINDOW, OnOpenMailWindow)
	ON_COMMAND(ID_COMMAND_HELPINDEX, OnHelpMenu)
	ON_MESSAGE(NSBUTTONMENUOPEN, OnButtonMenuOpen)
	ON_MESSAGE(TB_FILLINTOOLTIP, OnFillInToolTip)
	ON_MESSAGE(TB_FILLINSTATUS, OnFillInToolbarButtonStatus)
	ON_COMMAND(ID_VIEW_INCREASEFONT, OnIncreaseFont)
	ON_COMMAND(ID_VIEW_DECREASEFONT, OnDecreaseFont)
	ON_UPDATE_COMMAND_UI(ID_NETSEARCH, OnUpdateNetSearch)
	ON_COMMAND(ID_NETSEARCH, OnNetSearch)
	ON_UPDATE_COMMAND_UI(ID_SECURITY, OnUpdateSecurity)
	ON_UPDATE_COMMAND_UI(IDS_SECURITY_STATUS, OnUpdateSecurityStatus)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COMMANDTOOLBAR, OnUpdateViewCommandToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOCATIONTOOLBAR, OnUpdateViewLocationToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CUSTOMTOOLBAR, OnUpdateViewCustomToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NAVCENTER, OnUpdateViewNavCenter)

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction
CMainFrame::CMainFrame()
{
    m_pNext = NULL;
    m_pHistoryWindow = NULL;
    m_pDocInfoWindow = NULL;
	m_barLocation = NULL;
	m_barLinks = NULL;
	m_pCommandToolbar = NULL;

	m_tabFocusInMainFrm = TAB_FOCUS_IN_NULL ;
	m_SrvrItemCount = 0;
    m_bAutoMenuEnable = FALSE;  // enable all items by default -- needed for hotlist/etc which have no handler
}

CMainFrame::~CMainFrame()
{
	if(m_barLinks)
		delete m_barLinks;

	if(m_barLocation)
		delete m_barLocation;

	if(m_pCommandToolbar)
		delete m_pCommandToolbar;
}


//
// Create the ledges
//
BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
	//	Call the base, have it do the creation magic.
	BOOL bRetval = CGenericFrame::OnCreateClient(lpcs, pContext);
	if(bRetval == TRUE)	{
		//	Context Forever
		//	We know that the view that was made for the frame was the very last one
		//		created, use it to construct the context.
		//	Create a grid for each initial view.
        CWnd *pView = GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
        ASSERT(pView);
		CWinCX *pDontCare = new CWinCX((CGenericDoc *)pContext->m_pCurrentDoc,
			this, (CGenericView *)pView);
		SetMainContext(pDontCare);	//	This is the main context.
		SetActiveContext(pDontCare);	//	And the active one

		// cmanske: The following SetFocus() causes trouble for the Editor!
		// We don't have our pMWContext->is_editor flag set yet 
		//   and we have problems with JavaScript message processing
		//   caused by the SetFocus message.
		// So test for Edit frame and set the context flag now
#ifdef EDITOR
        if( IsEditFrame() && pDontCare->GetContext() ) {
            pDontCare->GetContext()->is_editor = TRUE;
        }
#endif //EDITOR
		// mwh - CDCCX::Initialize() will initialize the color palette, but we have to make sure we have 
		// the focus first for realizePalette()	to work.
		::SetFocus(pView->m_hWnd);
		RECT rect;
		GetClientRect(&rect);
		pDontCare->Initialize(pDontCare->CDCCX::IsOwnDC(), &rect);

#pragma message(__FILE__ ": move all this into the context constructor")
		pDontCare->GetContext()->fancyFTP = TRUE;
	    pDontCare->GetContext()->fancyNews = TRUE;
	    pDontCare->GetContext()->intrupt = FALSE;
	    pDontCare->GetContext()->reSize = FALSE;
	}
    return bRetval;    
}

int CMainFrame::m_FirstFrame = 1;

// Read the initial sizes out of preferences file
//
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	int16 iLeft,iRight,iTop,iBottom;
	PREF_GetRectPref("browser.window_rect", &iLeft, &iTop, &iRight, &iBottom);

    if (iLeft != -1) {
        cs.x = iLeft;
        cs.y = iTop;
        cs.cx = iRight - cs.x;
        cs.cy = iBottom - cs.y;
    }
	
    if(theApp.m_iCmdLnCX != -1) {
        cs.cx = theApp.m_iCmdLnCX;
    }
    if(theApp.m_iCmdLnCY != -1) {
        cs.cy = theApp.m_iCmdLnCY;
    }
    // no menu in kiosk mode.  duh.
    if (theApp.m_bKioskMode || theApp.m_ParentAppWindow)
        cs.hMenu = NULL;

    return(CGenericFrame::PreCreateWindow(cs));
}

                                          
//
// Initialize the Frame window
//
//#define DONT_DO_ANIM_PAL
#ifndef DONT_DO_ANIM_PAL
IL_RGB animationPalette[] =
    { //  R   G   B
		{255,255,204,0},
		{204,153,102,0},
		{255,102,51, 0},
        { 66,154,167,0},
		{  0, 55, 60,0},
		{  0,153,255,0},
		{ 51, 51,102,0},
		{128,  0,  0,0},
		{255,102,204,0},
		{153,  0,102,0},
		{153,102,204,0},
		{ 34, 34 ,34,0},
		{204,204,255,0},
		{153,153,255,0},
		{102,102,204,0},
		{ 51,255,153,0}
    };

int iLowerSystemColors = 10;
// int iLowerAnimationColors = 12;
int iLowerAnimationColors = 16;
int iLowerColors = 26; // iLowerSystemColors + iLowerAnimationColors;
int colorCubeSize = 216;

#endif

//      Toolbar width and heights.
#define MIXED_HEIGHT  32
#define MIXED_WIDTH   40
#define PICT_HEIGHT   21
#define PICT_WIDTH    23
#ifdef XP_WIN32
#define TEXT_HEIGHT     14
#define TEXT_WIDTH      42
#else
#define TEXT_HEIGHT     14
#define TEXT_WIDTH      49
#endif


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{

    if (CGenericFrame::OnCreate(lpCreateStruct) == -1)
        return -1;

    if( IsEditFrame() ){
    	// Note: Don't create the Browser toolbars in Editor frame,
        //   except for the status bar
		LPNSSTATUSBAR pIStatusBar = NULL;
		GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->Create( this );
			pIStatusBar->Release();
		}
    } else {
		
		//I'm hardcoding string since I don't want it translated.
	    GetChrome()->CreateCustomizableToolbar("Browser", 5, TRUE);

        // Now that the application palette has been created (if 
        //   appropriate) we can create the url bar.  The animation 
        //   might need custom colors so we need the palette to be around
        //
		CreateMainToolbar();

		if (!theApp.m_bInGetCriticalFiles) { // if we are here, don't show link bar
			CreateLocationBar();
			CreateLinkBar();  
			GetChrome()->FinishedAddingBrowserToolbars();
		}

		LPNSSTATUSBAR pIStatusBar = NULL;
		GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if( pIStatusBar ) 
		{
			pIStatusBar->Create( this );
			pIStatusBar->Release();
			if (theApp.m_ParentAppWindow || theApp.m_bKioskMode)
			    pIStatusBar->Show(FALSE);
		}

		if(!IsEditFrame())
		{
			AddToMenuMap(FILEMENU, IDM_MAINFRAMEFILEMENU);
			AddToMenuMap(EDITMENU, IDM_MAINFRAMEEDITMENU);
			AddToMenuMap(VIEWMENU, IDM_MAINFRAMEVIEWMENU);
			AddToMenuMap(GOMENU, IDM_MAINFRAMEGOMENU);
		}

    }

    //////////////////////////////////////////////////////////////////////////
    // Turn off widgets on demand
 
	m_iCSID = INTL_DefaultDocCharSetID(0); // Set Global default.

    m_iCSID = INTL_DefaultDocCharSetID(0); // Set Global default.

    //  Success so far.  Also, accept drag and drop files from
    //      the file manager.
    //
    DragAcceptFiles();

    return 0;
}

int CMainFrame::CreateLocationBar()
{


	m_barLocation=new CURLBar(); 

	if (!m_barLocation->Create(this, CURLBar::IDD, CBRS_TOP,CURLBar::IDD)) {
		return FALSE;
	}
	CToolbarWindow *pWindow = new CToolbarWindow(m_barLocation, theApp.m_pToolbarStyle, 23, 23, eSMALL_HTAB);

	BOOL bOpen, bShowing;
	int32 nPos;

	//I'm hardcoding because I don't want this translated
	GetChrome()->LoadToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"), nPos, bOpen, bShowing);

	if(PREF_PrefIsLocked("browser.chrome.show_url_bar"))
	{
		bShowing = m_bLocationBar;
	}

	GetChrome()->GetCustomizableToolbar()->AddNewWindow(ID_LOCATION_TOOLBAR, pWindow, nPos, 21, 21, 3, CString(szLoadString(ID_LOCATION_TOOLBAR)), theApp.m_pToolbarStyle, bOpen, FALSE);
	if (theApp.m_ParentAppWindow || theApp.m_bKioskMode)
	    GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, FALSE);
	else
	    GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, bShowing);

    m_barLocation->SetContext((LPUNKNOWN)GetMainContext());

	RecalcLayout();

	return TRUE;
}

int CMainFrame::CreateLinkBar(void)
{
	m_barLinks = CLinkToolbar::CreateUserToolbar(this);

	CButtonToolbarWindow *pWindow = new CButtonToolbarWindow(m_barLinks, theApp.m_pToolbarStyle, 43, 27, eSMALL_HTAB);

	BOOL bOpen, bShowing;

	int32 nPos;

	//I'm hardcoding because I don't want this translated
	GetChrome()->LoadToolbarConfiguration(ID_PERSONAL_TOOLBAR, CString("Personal_Toolbar"), nPos, bOpen, bShowing);

	if(PREF_PrefIsLocked("browser.chrome.show_directory_buttons"))
	{
		bShowing = m_bStarter;
	}

	GetChrome()->GetCustomizableToolbar()->AddNewWindow(ID_PERSONAL_TOOLBAR, pWindow, nPos, 43, 27, 1, CString(szLoadString(ID_PERSONAL_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
	if (theApp.m_ParentAppWindow || theApp.m_bKioskMode)
	    GetChrome()->ShowToolbar(ID_PERSONAL_TOOLBAR, FALSE);
	else
    	 GetChrome()->ShowToolbar(ID_PERSONAL_TOOLBAR, bShowing);

	RecalcLayout();

	return TRUE;
}

int CMainFrame::CreateMainToolbar(void)
{

	m_pCommandToolbar =new CCommandToolbar(15, theApp.m_pToolbarStyle, 43, 27, 27);



	if (!m_pCommandToolbar->Create(this))
	{
			return FALSE;
	}


	m_pCommandToolbar->SetBitmap(IDB_PICTURES);

	CButtonToolbarWindow *pWindow = new CButtonToolbarWindow(m_pCommandToolbar, theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);


	CString statusStr, toolTipStr, textStr;
	int nBitmapIndex = 0;

	int nCount = theApp.m_bShowNetscapeButton ? sizeof(buttons)/sizeof(UINT) : (sizeof(buttons)/sizeof(UINT)) - 1;

	for(int i = 0; i < nCount; i++)
	{
		CCommandToolbarButton *pCommandButton = new CCommandToolbarButton;

		WFE_ParseButtonString(buttons[i], statusStr, toolTipStr, textStr);

		DWORD dwButtonStyle = 0;

		if(buttons[i] == ID_NAVIGATE_FORWARD || buttons[i] == ID_NAVIGATE_BACK)
		{
			dwButtonStyle = TB_HAS_TIMED_MENU | TB_DYNAMIC_TOOLTIP;
		}
		else if(buttons[i] == ID_GO_HOME)
		{
			dwButtonStyle = TB_DYNAMIC_TOOLTIP;
		}
		else if(buttons[i] == ID_FILE_PRINT)
		{
			dwButtonStyle = TB_DYNAMIC_TOOLTIP | TB_DYNAMIC_STATUS;
		}
		else if(buttons[i] == ID_PLACES)
		{
			dwButtonStyle = TB_HAS_TIMED_MENU;
		}
		else if(buttons[i]== ID_NAVIGATE_INTERRUPT)
		{
			toolTipStr.LoadString(IDS_BROWSER_STOP_TIP);
		}
	
		pCommandButton->Create(m_pCommandToolbar, theApp.m_pToolbarStyle, CSize(44, 37), CSize(25, 25),
						(const char*) textStr,(const char*) toolTipStr, (const char*) statusStr,
						0, nBitmapIndex, CSize(23,21), buttons[i], -1, dwButtonStyle);

		pCommandButton->SetBitmap(m_pCommandToolbar->GetBitmap(), TRUE);

		m_pCommandToolbar->AddButton(pCommandButton, i);
		if(buttons[i] == ID_FILE_PRINT)
			nBitmapIndex+=2;

		nBitmapIndex++;
	}
	

	if(prefInfo.m_bAutoLoadImages)
	{
		m_pCommandToolbar->HideButtonByCommand(ID_VIEW_LOADIMAGES);
	}
	BOOL bOpen, bShowing;

	int32 nPos;

	//I'm hardcoding because I don't want this translated
	GetChrome()->LoadToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"), nPos, bOpen, bShowing);

	if(PREF_PrefIsLocked("browser.chrome.show_toolbar"))
	{
		bShowing = m_bShowToolbar;
	}
	GetChrome()->GetCustomizableToolbar()->AddNewWindow(ID_NAVIGATION_TOOLBAR, pWindow,nPos, 50, 37, 0, CString(szLoadString(ID_NAVIGATION_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);

	if (theApp.m_ParentAppWindow || theApp.m_bKioskMode)
	    GetChrome()->ShowToolbar(ID_NAVIGATION_TOOLBAR, FALSE);
	else
	    GetChrome()->ShowToolbar(ID_NAVIGATION_TOOLBAR, bShowing);
	

	RecalcLayout();

	return TRUE;
}

void CMainFrame::OnShowWindow (BOOL bShow, UINT nStatus)
{
    CGenericFrame::OnShowWindow(bShow,nStatus);
    // We don't seem to draw correctly as a child window on the initial pass.
    if(theApp.m_bChildWindow)
	RecalcLayout();
}

// returns TRUE if something was added to the folder, false otherwise
BOOL CMainFrame::FileBookmark(HT_Resource pFolder)
{

		History_entry *pHistEnt = SHIST_GetCurrent( &(GetMainContext()->GetContext()->hist) );

		if(pHistEnt)
		{

			HT_AddToContainer(pFolder, GetMainContext()->GetContext()->title, pHistEnt->address);
			return TRUE;
		}

		return FALSE;
}

//
// The window has been created and activated so the user can
//   see it.  Only now is it safe to load the home page
//
void CMainFrame::OnLoadHomePage()
{
#ifdef EDITOR
    if ( IsEditFrame() )
    {
        if ( theApp.m_CmdLineLoadURL ) {
            // Load command line URL, FALSE = do NOT start new window
            LoadUrlEditor(theApp.m_CmdLineLoadURL, FALSE);
            // The filename supplied with the -EDIT switch
            //   should NOT be stored as the "home page",
            //   load it only once then remove the URL
            XP_FREE(theApp.m_CmdLineLoadURL);
            theApp.m_CmdLineLoadURL = NULL;
        } else {
            // Start new doc with URL
            GetMainContext()->NormalGetUrl("about:editfilenew");
        }
        return;
    }
#endif /* EDITOR */
    
	LPCSTR	lpszHomePage = (LPCSTR)theApp.m_pHomePage;
    int32 	nStartup = 1;  // 0 = blank, 1 = home, 2 = last
	CString	strLastURL;
	CString csTmp;
	char *tmpBuf;
	XP_Bool bOverride = FALSE;

	PREF_GetBoolPref("browser.startup.homepage_override",&bOverride);

	if (bOverride) {
		// if the over-ride preference is set, use that instead
		PREF_CopyConfigString("startup.homepage_override_url",&tmpBuf);
		if (tmpBuf && tmpBuf[0]) {
			csTmp = tmpBuf;
			lpszHomePage = csTmp;
			XP_FREE(tmpBuf);
		}
		PREF_SetBoolPref("browser.startup.homepage_override",FALSE);
	}

    // If the user has specified a URL on the command line assume they want
    // to load it; otherwise don't load the homepage if the autoload entry
    // isn't "yes"
    if (theApp.m_CmdLineLoadURL && theApp.m_nChangeHomePage) {
        lpszHomePage = (LPCSTR)theApp.m_CmdLineLoadURL;

    } else {
        // There was neither a command line home page nor an Initial Home
        // Page entry so see if the user wants to load their default
        // home page or not
		if (theApp.m_bDontLoadHome)
			nStartup = 0;
		else
			PREF_GetIntPref("browser.startup.page", &nStartup);
    }

	// See if they want us to load the last page they viewed
	if (nStartup == 2) {
		if (GH_GetMRUPage(strLastURL.GetBufferSetLength(1024), 1024) > 0)
			lpszHomePage = (LPCSTR)strLastURL;
	}

    // If there is a home page location put it in the URL bar only if  we
    // autoload
    if (lpszHomePage) {
        if (nStartup != 0){
            GetMainContext()->NormalGetUrl(lpszHomePage);

			CURLBar *pUrlBar = (CURLBar *)GetChrome()->GetToolbar(ID_LOCATION_TOOLBAR);
			if(pUrlBar != NULL)
				pUrlBar->UpdateFields(lpszHomePage);
		}
    }
}

//////////////////////////////////////////////////////////////////////////////

void CMainFrame::OnDestroy()
{
	// The maps should be empty, because the dynamic menu items are
	// freed when the pop-up menus go away

    //////////////////////////////////////////////////////////////////////
    //  Clean up the location/starter button bars
    CGenericFrame::OnDestroy();
}


void CMainFrame::OnClose()
{
//  Purpose:            Close a window.
//  Arguments:          void
//  Return Value:       void
//  Comments:           Wrap up any processing of frame that
//                          needs to be handled, such as
//                          shutting down print preview.
//  Revision History:
//      11-06-94    created GAB
//
	// This frame still begin used by OLE server.
	if (HasSrvrItem()) {
		return;
	}
	//	See if the document will allow closing.
	//  Do not only query the active document as MFC does.
    //  Instead, query the main context document, and it will check children also.
	CDocument* pDocument = NULL;
    if(GetMainWinContext())    {
        pDocument = (CDocument *)GetMainWinContext()->GetDocument();
    }
	if (pDocument != NULL && !pDocument->CanCloseFrame(this))
	{
		// document can't close right now -- don't close it
		return;
	}

	//	Only save the window position if we're an browser context and the size wasn't chrome specified.
	//	This blocks saving the size/pos on things like html dialogs,
	//		view source, and document info.
	if(GetMainWinContext() && GetMainWinContext()->GetContext()->type == MWContextBrowser && 
	   GetMainWinContext()->m_bSizeIsChrome == FALSE)	{
	    WINDOWPLACEMENT wp;
        FEU_InitWINDOWPLACEMENT(GetSafeHwnd(), &wp);

		PREF_SetRectPref("browser.window_rect", (int)wp.rcNormalPosition.left,
			(int)wp.rcNormalPosition.top, (int)wp.rcNormalPosition.right,
			(int)wp.rcNormalPosition.bottom);

		//I'm hardcoding because I don't want this translated
		GetChrome()->SaveToolbarConfiguration(ID_NAVIGATION_TOOLBAR, CString("Navigation_Toolbar"));
		GetChrome()->SaveToolbarConfiguration(ID_LOCATION_TOOLBAR, CString("Location_Toolbar"));
		GetChrome()->SaveToolbarConfiguration(ID_PERSONAL_TOOLBAR, CString("Personal_Toolbar"));
		PREF_SetIntPref("browser.wfe.show_value",(int)wp.showCmd);
	}

	//	Tell our views to get rid of their contexts.
	if(GetMainWinContext())	{
		GetMainWinContext()->GetView()->FrameClosing();
	}

    //  Call base to close.
    //

    CGenericFrame::OnClose();
}

typedef struct drop_files_closure {
    HDROP	    hDropInfo;
    CMainFrame	  * pMain;
    char	 ** urlArray;
} drop_files_closure;

/* Mocha has processed the event handler for the drop event.  Now come back and execute
 * the drop files call if EVENT_OK or cleanup and go away.
 */
static void
win_drop_files_callback(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{
    HDROP hDropInfo;
    
    drop_files_closure * pClose = (drop_files_closure *) pObj;
    hDropInfo = pClose->hDropInfo;

    // make sure document hasn't gone away or event cancelled
    if(status == EVENT_PANIC || status == EVENT_CANCEL) {
	int i_loop=0;
	while ((pClose->urlArray)[i_loop])
	    XP_FREE((pClose->urlArray)[i_loop++]);
	XP_FREE(pClose->urlArray);
        XP_FREE(pClose);
        ::DragFinish(hDropInfo);
	return;
    }

    // find out who we are
    CMainFrame * pMain = pClose->pMain;

    //  First, check the number of files dropped.
    //
    auto int i_num = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

	// Get the current history entry
	History_entry * his = SHIST_GetCurrent(&(pMain->GetMainContext()->GetContext()->hist));

	// If we got a history entry, see if it's an
	// FTP directory listing.  If it is then lets ask the
	// user if they want to upload the dropped files.
	if(his 
		&& !strncasecomp(his->address, "ftp", 3)
		  && his->address[strlen(his->address)-1] == '/'
		    && FE_Confirm(pMain->GetMainContext()->GetContext(), szLoadString(IDS_UPLOAD_FILES)))
	  {
		// If the user wants to upload them, stuff the files
		// in the URL struct and start the load
		URL_Struct * URL_s = NET_CreateURLStruct(his->address, NET_SUPER_RELOAD);

		if(!URL_s)
			goto clean_up;

		URL_s->method = URL_POST_METHOD;
		URL_s->files_to_post = (char **)XP_ALLOC((i_num+1) * sizeof(char*));

		if(!URL_s->files_to_post)
		  {
			NET_FreeURLStruct(URL_s);
			goto clean_up;
		  }											 

		// load the file names into the files_to_post array
	  	auto char ca_drop[_MAX_PATH];
	  	for(auto int i_loop = 0; i_loop < i_num; i_loop++)
	  	  {
	  		::DragQueryFile(hDropInfo, i_loop, ca_drop, _MAX_PATH);

		  	URL_s->files_to_post[i_loop] = XP_STRDUP((const char*)ca_drop);
		  }
		URL_s->files_to_post[i_loop] = NULL; // terminate array

		pMain->GetMainContext()->GetUrl(URL_s, FO_CACHE_AND_PRESENT);
	  }
	else
	  {
    	//  Loop through them, each doing a load.
    	//  This should be insane fun with external viewers.
    	//
    	auto char ca_drop[_MAX_PATH];
		CString csUrl;
    	for(auto int i_loop = 0; i_loop < i_num; i_loop++)  {
	        ::DragQueryFile(hDropInfo, i_loop, ca_drop, _MAX_PATH);

    	    //  Convert to an acceptable URL.
        	//  It always has the full path!
        	//
			WFE_ConvertFile2Url(csUrl, ca_drop);

        	pMain->GetMainContext()->NormalGetUrl(csUrl);

	        //  Block until load is complete, if they did multiple
	        //      files.
	        //
	        if(i_loop != i_num - 1) {
	            auto int net_ret = 1;
	            winfeInProcessNet = TRUE;
	            while(net_ret)  {
	                net_ret = NET_ProcessNet(NULL, NET_EVERYTIME_TYPE);
	            }
	            winfeInProcessNet = FALSE;
	        }
	     }
      }

clean_up:
    //  Clean up
    //
    // Free mocha closure obj
	int i_loop=0;
	while ((pClose->urlArray)[i_loop])
	    XP_FREE((pClose->urlArray)[i_loop++]);
	XP_FREE(pClose->urlArray);
        XP_FREE(pClose);
        ::DragFinish(hDropInfo);

    //  Don't call this, or GPF, since we do it all anyways....
    //
    //CGenericFrame::OnDropFiles(hDropInfo);

}

void CMainFrame::OnDropFiles(HDROP hDropInfo)
{
    // TODO: Add your message handler code here and/or call default
    //

	//	Make sure we have a main context.
	if(GetMainContext() == NULL)	{
		return;
	}
#ifdef EDITOR
    // The view handles all dragNdrop in editor
    if( IsEditFrame() ){
        CWnd * pWnd = FromHandle(GetMainWinContext()->GetPane());
        if( pWnd ){
            ((CNetscapeEditView*)pWnd)->DropFiles(hDropInfo, TRUE);
        }
        return;
    }
#endif

    //  First, check the number of files dropped.
    //
    auto int i_num = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

    // Mocha sends an event here now to allow cancellation of OnDropFiles message.
    auto char ca_drop[_MAX_PATH];
    CString csUrl;
    int size = sizeof(char*) * (i_num + 1);
    char ** urlArray = (char**)XP_ALLOC(size);
    XP_ASSERT(urlArray);

    int i_loop;
    for(i_loop = 0; i_loop < i_num; i_loop++)  {
        ::DragQueryFile(hDropInfo, i_loop, ca_drop, _MAX_PATH);

	//  Convert to an acceptable URL.
	//  It always has the full path!
	//
	WFE_ConvertFile2Url(csUrl, ca_drop);
	urlArray[i_loop] = XP_STRDUP(csUrl);
    }
    urlArray[i_loop] = 0;

    drop_files_closure * pClosure = XP_NEW_ZAP(drop_files_closure);
    pClosure->pMain = this;
    pClosure->hDropInfo = hDropInfo;
    pClosure->urlArray = urlArray;

    // Get mouse at time of drop
    POINT pointDrop;

    ::DragQueryPoint( hDropInfo, &pointDrop);

    ClientToScreen( &pointDrop );

    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_DRAGDROP;
    event->screenx = pointDrop.x;
    event->screeny = pointDrop.y;
    event->modifiers = (GetKeyState(VK_SHIFT) < 0 ? EVENT_SHIFT_MASK : 0) 
		    | (GetKeyState(VK_CONTROL) < 0 ? EVENT_CONTROL_MASK : 0) 
		    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
    event->data = (void *)urlArray;
    event->dataSize = i_num;

    ET_SendEvent(GetMainContext()->GetContext(), 0, event, win_drop_files_callback, pClosure);
    return;

    //Mocha will handle cleanup and DragFinish when it calls back in.
    
}
