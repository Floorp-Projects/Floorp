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
 *   Chak Nanga <chak@netscape.com> 
 */

// File Overview....
//
// The typical MFC View, toolbar, statusbar creation done 
// in CBrowserFrame::OnCreate()
//
// Code to update the Status/Tool bars in response to the
// Web page loading progress(called from methods in CBrowserImpl)
//
// SetupFrameChrome() determines what, if any, UI elements this Frame
// will sport based on the current "chromeMask" 
//
// Also take a look at OnClose() which gets used when you close a browser
// window. This needs to be overrided mainly to handle supporting multiple
// browser frame windows via the "New Browser Window" menu item
// Without this being overridden the MFC framework handles the OnClose and
// shutsdown the complete application when a frame window is closed.
// In our case, we want the app to shutdown when the File/Exit menu is chosen
//
// Another key functionality this object implements is the IBrowserFrameGlue
// interface - that's the interface the Gecko embedding interfaces call
// upong to update the status bar etc.
// (Take a look at IBrowserFrameGlue.h for the interface definition and
// the BrowserFrm.h to see how we implement this interface - as a nested
// class)
// We pass this Glue object pointer to the CBrowserView object via the 
// SetBrowserFrameGlue() method. The CBrowserView passes this on to the
// embedding interface implementaion
//
// Please note the use of the macro METHOD_PROLOGUE in the implementation
// of the nested BrowserFrameGlue object. Essentially what this macro does
// is to get you access to the outer (or the object which is containing the
// nested object) object via the pThis pointer.
// Refer to the AFXDISP.H file in VC++ include dirs
//
// Next suggested file to look at : BrowserView.cpp

#include "stdafx.h"
#include "MfcEmbed.h"
#include "BrowserFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBrowserFrame

IMPLEMENT_DYNAMIC(CBrowserFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CBrowserFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CBrowserFrame)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_ACTIVATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // For the Status line
	ID_SEPARATOR,           // For the Progress Bar
	ID_SEPARATOR,           // For the padlock image
};

/////////////////////////////////////////////////////////////////////////////
// CBrowserFrame construction/destruction

CBrowserFrame::CBrowserFrame(PRUint32 chromeMask)
{
	// Save the chromeMask off. It'll be used
	// later to determine whether this browser frame
	// will have menubar, toolbar, statusbar etc.

	m_chromeMask = chromeMask;
}

CBrowserFrame::~CBrowserFrame()
{
}

void CBrowserFrame::OnClose()
{
	CMfcEmbedApp *pApp = (CMfcEmbedApp *)AfxGetApp();
	pApp->RemoveFrameFromList(this);

	DestroyWindow();
}

// This is where the UrlBar, ToolBar, StatusBar, ProgressBar
// get created
//
int CBrowserFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// Pass "this" to the View for later callbacks
	// and/or access to any public data members, if needed
	//
	m_wndBrowserView.SetBrowserFrame(this);

	// Pass on the BrowserFrameGlue also to the View which
	// it will use during the Init() process after creation
	// of the BrowserImpl obj. Essentially, the View object
	// hooks up the Embedded browser's callbacks to the BrowserFrame
	// via this BrowserFrameGlue object
	m_wndBrowserView.SetBrowserFrameGlue((PBROWSERFRAMEGLUE)&m_xBrowserFrameGlueObj);

	// create a view to occupy the client area of the frame
	// This will be the view in which the embedded browser will
	// be displayed in
	//
	if (!m_wndBrowserView.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	// create the URL bar (essentially a ComboBoxEx object)
	if (!m_wndUrlBar.Create(CBS_DROPDOWN | WS_CHILD, CRect(0, 0, 200, 150), this, ID_URL_BAR))
	{
		TRACE0("Failed to create URL Bar\n");
		return -1;      // fail to create
	}
    
    // Load the Most Recently Used(MRU) Urls into the UrlBar
    m_wndUrlBar.LoadMRUList();

	// Create the toolbar with Back, Fwd, Stop, etc. buttons..
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	// Create a ReBar window to which the toolbar and UrlBar 
	// will be added
	if (!m_wndReBar.Create(this))
	{
		TRACE0("Failed to create ReBar\n");
		return -1;      // fail to create
	}
	
	//Add the ToolBar and UrlBar windows to the rebar
	m_wndReBar.AddBar(&m_wndToolBar);
	m_wndReBar.AddBar(&m_wndUrlBar, "Enter URL:");

	// Create the status bar with two panes - one pane for actual status
	// text msgs. and the other for the progress control
	if (!m_wndStatusBar.CreateEx(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// Create the progress bar as a child of the status bar.
	// Note that the ItemRect which we'll get at this stage
	// is bogus since the status bar panes are not fully
	// positioned yet i.e. we'll be passing in an invalid rect
	// to the Create function below
	// The actual positioning of the progress bar will be done
	// in response to OnSize()
	RECT rc;
	m_wndStatusBar.GetItemRect (1, &rc);
	if (!m_wndProgressBar.Create(WS_CHILD|WS_VISIBLE|PBS_SMOOTH, rc, &m_wndStatusBar, ID_PROG_BAR))
	{
		TRACE0("Failed to create progress bar\n");
		return -1;      // fail to create
	}

	// The third pane(i.e. at index 2) of the status bar will have 
	// the security lock icon displayed in it. Set up it's size(16) 
	// and style(no border)so that the padlock icons can be properly drawn
	m_wndStatusBar.SetPaneInfo(2, -1, SBPS_NORMAL|SBPS_NOBORDERS, 16);

	// Also, set the padlock icon to be the insecure icon to begin with
	UpdateSecurityStatus(nsIWebProgressListener::STATE_IS_INSECURE);

	// Based on the "chromeMask" we were supplied during construction
	// hide any requested UI elements - statusbar, menubar etc...
	// Note that the window styles (WM_RESIZE etc) are set inside
	// of PreCreateWindow()

	SetupFrameChrome(); 

	return 0;
}

void CBrowserFrame::SetupFrameChrome()
{
	if(m_chromeMask == nsIWebBrowserChrome::CHROME_ALL)
		return;

	if(! (m_chromeMask & nsIWebBrowserChrome::CHROME_MENUBAR) )
		SetMenu(NULL); // Hide the MenuBar

	if(! (m_chromeMask & nsIWebBrowserChrome::CHROME_TOOLBAR) )
		m_wndReBar.ShowWindow(SW_HIDE); // Hide the ToolBar

	if(! (m_chromeMask & nsIWebBrowserChrome::CHROME_STATUSBAR) )
		m_wndStatusBar.ShowWindow(SW_HIDE); // Hide the StatusBar
}

BOOL CBrowserFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	// Change window style based on the chromeMask

	if(! (m_chromeMask & nsIWebBrowserChrome::CHROME_TITLEBAR) )
		cs.style &= ~WS_CAPTION; // No caption		

	if(! (m_chromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) )
	{
		// Can't resize this window
		cs.style &= ~WS_SIZEBOX;
		cs.style &= ~WS_THICKFRAME;
		cs.style &= ~WS_MINIMIZEBOX;
		cs.style &= ~WS_MAXIMIZEBOX;
	}

	cs.lpszClass = AfxRegisterWndClass(0);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CBrowserFrame message handlers
void CBrowserFrame::OnSetFocus(CWnd* pOldWnd)
{
	// forward focus to the view window
	m_wndBrowserView.SetFocus();
}

BOOL CBrowserFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	if (m_wndBrowserView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

// Needed to properly position/resize the progress bar
//
void CBrowserFrame::OnSize(UINT nType, int cx, int cy) 
{
       CFrameWnd::OnSize(nType, cx, cy);
	
       // Get the ItemRect of the status bar's Pane 1
       // That's where the progress bar will be located
       RECT rc;
       m_wndStatusBar.GetItemRect(1, &rc);

       // Move the progress bar into it's correct location
       //
       m_wndProgressBar.MoveWindow(&rc);
}

#ifdef _DEBUG
void CBrowserFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CBrowserFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG


void CBrowserFrame::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
	CFrameWnd::OnActivate(nState, pWndOther, bMinimized);
	
    m_wndBrowserView.Activate(nState, pWndOther, bMinimized);
}

#define IS_SECURE(state) ((state & 0xFFFF) == nsIWebProgressListener::STATE_IS_SECURE)
void CBrowserFrame::UpdateSecurityStatus(PRInt32 aState)
{
    int iResID = nsIWebProgressListener::STATE_IS_INSECURE;
    
    if(IS_SECURE(aState)){
        iResID = IDR_SECURITY_LOCK;
        m_wndBrowserView.m_SecurityState = CBrowserView::SECURITY_STATE_SECURE;
    }
    else if(aState == nsIWebProgressListener::STATE_IS_INSECURE) {
        iResID = IDR_SECURITY_UNLOCK;
        m_wndBrowserView.m_SecurityState = CBrowserView::SECURITY_STATE_INSECURE;
    }
    else if(aState == nsIWebProgressListener::STATE_IS_BROKEN) {
        iResID = IDR_SECURITY_BROKEN;
        m_wndBrowserView.m_SecurityState = CBrowserView::SECURITY_STATE_BROKEN;
    }

    CStatusBarCtrl& sb = m_wndStatusBar.GetStatusBarCtrl();
    sb.SetIcon(2, //2 is the pane index of the status bar where the lock icon will be shown
        (HICON)::LoadImage(AfxGetResourceHandle(),
        MAKEINTRESOURCE(iResID), IMAGE_ICON, 16,16,0));       
}

void CBrowserFrame::ShowSecurityInfo()
{   
    m_wndBrowserView.ShowSecurityInfo();
}

// CMyStatusBar Class
CMyStatusBar::CMyStatusBar()
{
}

CMyStatusBar::~CMyStatusBar()
{
}

BEGIN_MESSAGE_MAP(CMyStatusBar, CStatusBar)
	//{{AFX_MSG_MAP(CMyStatusBar)
	ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CMyStatusBar::OnLButtonDown(UINT nFlags, CPoint point) 
{
    // Check to see if the mouse click was within the
    // padlock icon pane(at pane index 2) of the status bar...

    RECT rc;
    GetItemRect(2, &rc );

    if(PtInRect(&rc, point)) 
    {
        CBrowserFrame *pFrame = (CBrowserFrame *)GetParent();
        if(pFrame != NULL)
            pFrame->ShowSecurityInfo();
    }
    	
	CStatusBar::OnLButtonDown(nFlags, point);
}
