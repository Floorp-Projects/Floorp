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

/* COMPFRM.CPP - Compose Window Frame.  This module contains code for the
 * compose window frame class.  Handles all menu and tool commands.
 *
 */
 
#include "stdafx.h"

#include "rosetta.h"
#include "edt.h"
#include "compbar.h"
#include "compfrm.h"
#include "prefapi.h"
#include "addrdlg.h"
#include "intl_csi.h"
#include "apiaddr.h"
#include "namcomp.h"
#include "addrfrm.h"  //for MOZ_NEWADDR
  
#include "nscpmapi.h"   // rhp: for MAPI check

extern "C" {
#include "xpgetstr.h"
extern int MK_MSG_SAVE_AS;
extern int MK_MSG_CANT_OPEN;
extern int MK_MSG_MISSING_SUBJECT;
};

#ifdef XP_WIN32
#include "postal.h"
#endif


#define COMPOSE_WINDOW_WIDTH    73 // default to 73 characters wide

/////////////////////////////////////////////////////////////////////////////
// CComposeFrame

// This is for mail that is sent immediately
static UINT ComposeCodes[] = {
   IDM_SENDNOW,
   IDM_QUOTEORIGINAL,
   IDM_ADDRESSPICKER,
   ID_CHECK_SPELLING,
   IDM_SAVEASDRAFT,
   HG26722
   ID_NAVIGATE_INTERRUPT
    };

static BOOL bSendDeferred = FALSE;

const UINT NEAR MAPI_CUST_MSG = RegisterWindowMessage(MAPI_CUSTOM_COMPOSE_MSG); // rhp - for MAPI

BEGIN_MESSAGE_MAP(CComposeFrame, CGenericFrame)
   ON_WM_CREATE()
   ON_WM_CLOSE()
   ON_WM_SETFOCUS()
   ON_MESSAGE(WM_SETMESSAGESTRING, OnSetMessageString)
   ON_MESSAGE(NSBUTTONMENUOPEN, OnButtonMenuOpen)
   ON_MESSAGE(WM_TOOLCONTROLLER,OnToolController)
   ON_COMMAND(ID_CHECK_SPELLING, OnCheckSpelling)
   ON_COMMAND(IDM_PASTEASQUOTE, OnPasteQuote)
   ON_UPDATE_COMMAND_UI(IDM_PASTEASQUOTE, OnUpdatePasteQuote)
   ON_COMMAND(ID_EDIT_SELECTALLMESSAGES, OnSelectAll)
   ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALLMESSAGES,OnUpdateSelectAll)
   ON_COMMAND(IDM_SEND, OnSend)
   ON_UPDATE_COMMAND_UI(IDM_SEND, OnUpdateSend)
   ON_COMMAND(IDM_SENDNOW, OnSendNow)
   ON_UPDATE_COMMAND_UI(IDM_SENDNOW, OnUpdateSendNow)
   ON_COMMAND(IDM_SENDLATER, OnSendLater)
   ON_UPDATE_COMMAND_UI(IDM_SENDLATER,OnUpdateSendLater)
   ON_COMMAND(IDM_QUOTEORIGINAL, OnQuoteOriginal)
   ON_UPDATE_COMMAND_UI(IDM_QUOTEORIGINAL,OnUpdateButtonGeneral)
   ON_COMMAND(IDM_SAVEAS, OnSaveAs)
   ON_UPDATE_COMMAND_UI(IDM_SAVEAS, OnUpdateButtonGeneral)
   ON_COMMAND(IDM_SAVEASDRAFT, OnSaveDraft)
   ON_UPDATE_COMMAND_UI(IDM_SAVEASDRAFT, OnUpdateSaveDraft)
   ON_COMMAND(IDM_SAVEASTEMPLATE, OnSaveTemplate)
   ON_UPDATE_COMMAND_UI(IDM_SAVEASTEMPLATE, OnUpdateSaveTemplate)
   ON_COMMAND(IDM_CONVERT,OnConvert)
   ON_UPDATE_COMMAND_UI(IDM_CONVERT, OnUpdateConvert)
   ON_COMMAND(IDM_ATTACHFILE,OnAttachFile)
   ON_COMMAND(ID_SECURITY,OnSecurity)
   ON_UPDATE_COMMAND_UI(ID_SECURITY,OnUpdateSecurity)
   
   ON_COMMAND(ID_DONEGOINGOFFLINE, OnDoneGoingOffline)

   ON_WM_DESTROY()
   ON_BN_CLICKED(IDM_ATTACHFILE, OnAttachFile)
   ON_BN_CLICKED(IDM_ATTACHWEBPAGE, OnAttachUrl)
   ON_COMMAND(IDM_ADDRESSPICKER,OnSelectAddresses)
   ON_COMMAND(ID_MAIL_WRAPLONGLINES,OnWrapLongLines)
   ON_UPDATE_COMMAND_UI(ID_MAIL_WRAPLONGLINES,OnUpdateWrapLongLines)    
   ON_COMMAND(IDM_ATTACHMYCARD,OnAttachMyCard)
   ON_UPDATE_COMMAND_UI(IDM_ATTACHMYCARD,OnUpdateAttachMyCard)
   ON_COMMAND(ID_FILE_NEWMESSAGE, OnNew)
   ON_COMMAND(IDM_ADDRESSES,OnViewAddresses)
   ON_COMMAND(IDM_ATTACHMENTS,OnViewAttachments)
   ON_COMMAND(IDM_OPTIONS,OnViewOptions)
   ON_COMMAND(IDC_ADDRESSTAB,OnAddressTab)
   ON_COMMAND(IDC_ATTACHTAB,OnAttachTab)
   ON_COMMAND(IDC_OPTIONSTAB,OnOptionsTab)
   ON_COMMAND(IDC_COLLAPSE,OnCollapse)
   ON_UPDATE_COMMAND_UI(ID_CHECK_SPELLING, OnUpdateButtonGeneral)
   ON_UPDATE_COMMAND_UI(IDM_ADDRESSES,OnUpdateViewAddresses)
   ON_UPDATE_COMMAND_UI(IDM_ATTACHMENTS,OnUpdateViewAttachments)
   ON_UPDATE_COMMAND_UI(IDM_OPTIONS,OnUpdateViewOptions)
   ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)
   // general button enabler, these should be enabled as long as the
   // compose window is visible
   ON_UPDATE_COMMAND_UI(ID_VIEW_ADDRESSES, OnUpdateButtonGeneral)
   ON_UPDATE_COMMAND_UI(IDC_ADDRESS,OnUpdateButtonGeneral)
   ON_UPDATE_COMMAND_UI(IDC_DIRECTORY,OnUpdateButtonGeneral)
   ON_UPDATE_COMMAND_UI(IDM_ATTACHFILE,OnUpdateButtonGeneral) 
   ON_UPDATE_COMMAND_UI(IDM_ATTACHWEBPAGE,OnUpdateButtonGeneral)
   ON_UPDATE_COMMAND_UI(IDM_ADDRESSPICKER,OnUpdateButtonGeneral) 

   ON_COMMAND(IDM_MESSAGEBODYONLY,OnToggleAddressArea)
   ON_UPDATE_COMMAND_UI(IDM_MESSAGEBODYONLY,OnUpdateToggleAddressArea)
   ON_COMMAND(IDM_OPT_MESSAGEBAR_TOGGLE, OnToggleMessageToolbar)
   ON_UPDATE_COMMAND_UI(IDM_OPT_MESSAGEBAR_TOGGLE, OnUpdateToggleMessageToolbar)

	// Security Stuff 
   	ON_UPDATE_COMMAND_UI(IDS_SECURITY_STATUS, OnUpdateSecureStatus)
	ON_UPDATE_COMMAND_UI(IDS_SIGNED_STATUS, OnUpdateSecureStatus)

	// Name completion 
	ON_UPDATE_COMMAND_UI(ID_TOGGLENAMECOMPLETION, OnUpdateToggleNameCompletion)
	ON_COMMAND(ID_TOGGLENAMECOMPLETION, OnToggleNameCompletion)
	ON_UPDATE_COMMAND_UI(ID_SHOW_NAME_PICKER, OnUpdateShowNamePicker)
	ON_COMMAND(ID_SHOW_NAME_PICKER, OnShowNamePicker)

// rhp - for custom message
#ifndef MOZ_LITE
   ON_REGISTERED_MESSAGE(MAPI_CUST_MSG, OnProcessMAPIMessage)    // rhp - for MAPI
#endif

END_MESSAGE_MAP()

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CComposeFrame,CGenericFrame)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#ifdef XP_WIN32
char *ToolBarProfileName = "MailComposeToolBar";
#endif

// Constructor for CCompFrame.  Creates the toolbar controller object.
// This object is used by the Gold editor to display/control the
// HTML toolbar commands.  Initialize local variables, set the deferred
// flag to the last known state

CComposeFrame::CComposeFrame()
{
	PREF_GetBoolPref("network.online", &bSendDeferred);
   bSendDeferred = !bSendDeferred;
	m_bUseHtml = FALSE;
	m_pToolBarController = NULL;
   m_pComposeBar = NULL;                               // dialog bar
	m_pComposePane = NULL;                              // message pane (for backend)
   m_pFocus = NULL;                                    // current field with focus
   m_bInitialized = FALSE;
	m_pInitialText = NULL;
   m_bWrapLongLines = TRUE;
   m_cxChar = 0;
   m_quoteSel = -1;				// no quoting selection yet

   m_bMAPISendMode = MAPI_IGNORE;   // rhp - For MAPI UI-less send/save operations
}

// The menu gets cleaned up on window destruction but we need
// to delete our accelerator table ourselves

CComposeFrame::~CComposeFrame()
{
    // free the accelerator table
    if(m_hAccelTable != NULL)   {
    	::FreeResource((HGLOBAL)m_hAccelTable);
    	m_hAccelTable = NULL;
    }
    if (m_pToolBarController)
    	delete m_pToolBarController;
    // free up the dialog bar (address & attachment controls)
    if (m_pComposeBar)
    	delete m_pComposeBar;
}


// GetAddressWidget() returns the current address control widget.

LPADDRESSCONTROL CComposeFrame::GetAddressWidgetInterface() 
{
    ASSERT(m_pComposeBar);
    return m_pComposeBar->GetAddressWidgetInterface();
}    

// AppendAddress() adds a header/value to the current address block.  It 
// accepts a MSG_HEADER_SET value which is translated to the appropriate 
// corresponding string and appended to the end of the address list with 
// the supplied value.
// Returns TRUE on success, FALSE otherwise.

BOOL CComposeFrame::AppendAddress(
    MSG_HEADER_SET header,  // header to add
    const char * value      // header value (or NULL for empty)
    )
{
    // check for a valid widget 
    if (::IsWindow(m_pComposeBar->m_hWnd))
    {
	    // get a pointer to the addressing block widget
        LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
        if (pIAddress)
        {
            CListBox * pListBox = pIAddress->GetListBox();	    
    	    // check to make sure we got a valid list and the list
    	    // has a valid window
    	    if (::IsWindow(pListBox->m_hWnd))
    	    {
                const char * szType = MSG_HeaderMaskToString(header);
                if (!szType)
                    return FALSE;
    	        // add the entry to the list
                pIAddress->AppendEntry(TRUE, szType,value ? value : "", 0, 0);
    	    }
        }
    }
    return TRUE;
}

// destroy message compose pane
void CComposeFrame::OnDestroy()
{
    MSG_DestroyPane(m_pComposePane);
    m_pComposePane = NULL;
    CGenericFrame::OnDestroy();
}

int CComposeFrame::CreateHtmlToolbars()
{
   // Create the HTML edit toolbars.  There are currently two separate
   // toolbars.. one for formats and another for character operations.
   m_pToolBarController = new CEditToolBarController(this);
   ASSERT(m_pToolBarController);
   if (!m_pToolBarController->CreateEditBars(GetMainContext()->GetContext(), DISPLAY_CHARACTER_TOOLBAR))
      return -1;      // fail to create
   return 0;
}

void CComposeFrame::DestroyHtmlToolbars()
{
   XP_Bool prefBool;
   PREF_GetBoolPref("editor.show_character_toolbar",&prefBool);
   if ( prefBool ) 
   {
      CWnd * pWnd = m_pToolBarController->GetCharacterBar();
      if (pWnd && ::IsWindow(pWnd->m_hWnd))
         pWnd->DestroyWindow();
   }
}

void CComposeFrame::ShowHtmlToolbars()
{
   // display the appropriate toolbars for HTML editing based on 
   // configuration.
   XP_Bool prefBool;
   PREF_GetBoolPref("editor.show_character_toolbar",&prefBool);
   if ( prefBool ) 
      m_pToolBarController->GetCharacterBar()->ShowWindow(SW_SHOW);
}

void CComposeFrame::SetMsgPane(MSG_Pane * pPane) 
{ 
    m_pComposePane = pPane; 
}

void CComposeFrame::CreatePlainTextEditor()
{
   CWnd *pView = GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
   ASSERT(pView);

   // create the editor parent.  This is a window whose sole purpose is to
   // resize the edit control... this will go away when the Gold editor
   // is fully integrated.

   CRect rect(0,0,0,0);   
   if (::IsWindow(pView->m_hWnd) && pView->IsWindowVisible())
      pView->GetClientRect(rect);
   
   m_EditorParent.Create ( 
      NULL, NULL, WS_VISIBLE | WS_CHILD,  rect, pView, 0 );        

   SetEditorParent((CWnd*)&m_EditorParent);

   // create the editor which is currently a standard windows edit 
   // control.  This will be replaced by the Gold editor.

   if (rect.Width())
      rect.left = EDIT_MARGIN_OFFSET;

   m_Editor.Create ( 
      WS_VSCROLL | ES_MULTILINE | WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
      rect, 
      &m_EditorParent, IDC_COMPOSEEDITOR);
      
   m_Editor.SetComposeFrame(this);
   m_Editor.SetTabStops();

   m_cxChar = m_Editor.GetCharWidth();

   #ifdef XP_WIN16
   m_Editor.SendMessage(EM_LIMITTEXT,0,0);
   #endif
}

// Perform initial setup for the compose window.

// status bar format
static const UINT BASED_CODE indicators[] =
{
	HG27811
    IDS_TRANSFER_STATUS,    
    ID_SEPARATOR,
	IDS_ONLINE_STATUS,
	IDS_TASKBAR
};

int CComposeFrame::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
   int retval = CGenericFrame::OnCreate ( lpCreateStruct );

	//I'm hardcoding string since I don't want it translated.
	m_pChrome->CreateCustomizableToolbar("Compose_Message", 3, TRUE);
	CButtonToolbarWindow *pWindow;
	BOOL bOpen, bShowing;

	int32 nPos;

	m_pChrome->LoadToolbarConfiguration(ID_COMPOSE_MESSAGE_TOOLBAR, CString("Message_Toolbar"),nPos, bOpen, bShowing);

	// Set up toolbar
	LPNSTOOLBAR pIToolBar = NULL;
	m_pChrome->QueryInterface(IID_INSToolBar,(LPVOID*)&pIToolBar);
	if ( pIToolBar ) 
   {
		pIToolBar->Create( this );
		pIToolBar->SetButtons( ComposeCodes, sizeof(ComposeCodes) / sizeof(UINT) );
		pIToolBar->SetSizes( CSize( 23+7, 21+6), CSize( 23, 21 ) );
	   // this is currently loading one bitmap.. this needs to be fixed to add
	   // the deferred bitmaps as well.
		pIToolBar->LoadBitmap(MAKEINTRESOURCE(IDB_COMPOSETOOLBAR));
		pIToolBar->SetToolbarStyle( theApp.m_pToolbarStyle );
		pWindow = new CButtonToolbarWindow(CWnd::FromHandlePermanent(pIToolBar->GetHWnd()), theApp.m_pToolbarStyle, 43, 27, eLARGE_HTAB);
		m_pChrome->GetCustomizableToolbar()->AddNewWindow(ID_COMPOSE_MESSAGE_TOOLBAR, pWindow,nPos, 50, 37, 0, CString(szLoadString(ID_COMPOSE_MESSAGE_TOOLBAR)),theApp.m_pToolbarStyle, bOpen, FALSE);
		m_pChrome->ShowToolbar(ID_COMPOSE_MESSAGE_TOOLBAR, bShowing);
		m_pChrome->GetCustomizableToolbar()->SetBottomBorder(TRUE);
		CWnd *pToolWnd = CWnd::FromHandlePermanent( pIToolBar->GetHWnd() );
		if ( pToolWnd ) {
            CStationaryToolbarButton * pStationaryToolbarButton = new CStationaryToolbarButton;
            pStationaryToolbarButton->Create(pToolWnd, theApp.m_pToolbarStyle, CSize(50, 40), CSize(30, 27), 
               szLoadString(IDS_TOOLATTACH),
               szLoadString(IDS_TOOLATTACHFILES),
               szLoadString(IDS_TOOLATTACHALL),
					IDB_ATTACHBUTTON, 0, CSize(23, 21), TRUE, IDM_ATTACHFILE, 10, TB_HAS_IMMEDIATE_MENU);
			pIToolBar->AddButton( pStationaryToolbarButton, 3 );
		}

      if (bSendDeferred)
      {
           pIToolBar->RemoveButton(0);
   		   CCommandToolbarButton *pCommandButton = new CCommandToolbarButton;
       	   CString statusStr, toolTipStr, textStr;
		   WFE_ParseButtonString(IDM_SENDLATER, statusStr, toolTipStr, textStr);
		   DWORD dwButtonStyle = 0;
		   pCommandButton->Create(pToolWnd, theApp.m_pToolbarStyle, CSize(44, 37), CSize(25, 25),
						(const char*) textStr,(const char*) toolTipStr, (const char*) statusStr,
						IDB_COMPOSETOOLBAR, 7, CSize(23,21), IDM_SENDLATER, -1, dwButtonStyle);
		   pIToolBar->AddButton(pCommandButton, 0);
		   CCommandToolbar *pToolBar = (CCommandToolbar *) CWnd::FromHandlePermanent( pIToolBar->GetHWnd() );
           pToolBar->ReplaceButtonBitmapIndex(IDM_SENDNOW,7);
      }

    	m_pChrome->Release();
	}

   // create the address/attachment widget dialog bar
   m_pComposeBar = new CComposeBar(GetMainContext()->GetContext());
   ASSERT(m_pComposeBar);
   m_pComposeBar->Create ( this, (UINT) IDD_COMPOSEBAR, 
						   (UINT) WS_CLIPCHILDREN|CBRS_TOP, 
		                   (UINT) IDD_COMPOSEBAR );

   if (m_bUseHtml)
   {
      if (CreateHtmlToolbars() == -1)
         return -1;
        
      RecalcLayout();        
      // Check if format toolbar needs more room and 
      // increase window width to fit it, but observe 600 pixel maximum width
      int iWidth = min(600, m_pToolBarController->GetCharacterBar()->GetToolbarWidth()
                       + GetSystemMetrics(SM_CXFRAME) + 8); // Kludge - 8 more needed to work right!

      if( iWidth > lpCreateStruct->cx ){
         SetWindowPos(NULL, 0, 0, iWidth,
                      lpCreateStruct->cy, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
      }
   }
   else
   {
      CreatePlainTextEditor();
   }

   // create a unique title for the composition window    
   CString cs;
   cs = szLoadString(IDS_COMPOSITION);
   GetChrome()->SetWindowTitle(cs);

    // Instead of using ANSI_VAR_FONT for i18n
	SetCSID(INTL_DefaultWinCharSetID(0));

   // create the status bar
	m_barStatus.Create( this );
	m_barStatus.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	LPNSSTATUSBAR pIStatusBar = NULL;
	m_pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
	if ( pIStatusBar ) {
		pIStatusBar->Attach( &m_barStatus );
		pIStatusBar->Release();
	}

   if (m_bUseHtml)
      ShowHtmlToolbars();

    // Attach the submenus shared by Composer, Message Composer, and right-button popups
    HMENU hMenu = ::GetMenu(m_hWnd);
    if( hMenu )
    {
        HMENU hTableMenu = ::GetSubMenu(hMenu, ED_MENU_TABLE);
        if( hTableMenu )
        {
            ::ModifyMenu( hTableMenu, ID_SELECT_TABLE, MF_BYCOMMAND | MF_POPUP | MF_STRING,
                          (UINT)::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDM_COMPOSER_TABLE_SELECTMENU)),
                          szLoadString(IDS_SUBMENU_SELECT_TABLE) );
            ::ModifyMenu( hTableMenu, ID_INSERT_TABLE, MF_BYCOMMAND | MF_POPUP | MF_STRING,
                          (UINT)::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDM_COMPOSER_TABLE_INSERTMENU)),
                          szLoadString(IDS_SUBMENU_INSERT_TABLE) );
            ::ModifyMenu( hTableMenu, ID_DELETE_TABLE, MF_BYCOMMAND | MF_POPUP | MF_STRING,
                          (UINT)::LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDM_COMPOSER_TABLE_DELETEMENU)),
                          szLoadString(IDS_SUBMENU_DELETE_TABLE) );
        }
    }
   return retval;
}

// OnCreateClient() sets up the context information and create the
// contained CView derived class.

BOOL CComposeFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
	 BOOL bRetval = CGenericFrame::OnCreateClient(lpcs, pContext);

    // Get the current view
    CWnd *pView = GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
    ASSERT(pView);
	 CWinCX * pWinCX = new CWinCX(
	   (CGenericDoc*)pContext->m_pCurrentDoc, this, (CGenericView *)pView );

	 //      This is the main and active context.
	 SetMainContext(pWinCX);
	 SetActiveContext(pWinCX);
	RECT rect;
	GetClientRect(&rect);
    pWinCX->Initialize(FALSE, &rect);
    pWinCX->GetContext()->fancyFTP = TRUE;
    pWinCX->GetContext()->fancyNews = TRUE;
    pWinCX->GetContext()->intrupt = FALSE;
    if (m_bUseHtml)
	      pWinCX->GetContext()->is_editor = TRUE;

    pWinCX->GetContext()->reSize = FALSE;
    return bRetval;
}

// PreCreateWindow() sets the initial compose window size based on
// values that were saved in the registry during OnClose().  The width
// is currently hard-coded to open up to 73 (COMPOSE_WINDOW_WIDTH)
// characters wide.

BOOL CComposeFrame::PreCreateWindow ( CREATESTRUCT &cs )
{
	cs.style |= WS_CLIPCHILDREN;

    BOOL rv = CGenericFrame::PreCreateWindow ( cs );

	int16 iLeft,iRight,iTop,iBottom;
	PREF_GetRectPref("mail.compose_window_rect", &iLeft, &iTop, &iRight, &iBottom);
	// Restore X,Y and height, but recalculate width
    if ( iLeft != -1) 
    {
		cs.x = iLeft;
		cs.y = iTop;
		cs.cy = iBottom-iTop;

    //if (m_bUseHtml)
    //{
      cs.cx = iRight - iLeft;
      return rv;
    //}
	} 

    CWnd *pCwnd = AfxGetMainWnd();
    ASSERT(pCwnd);
    
    // get the current character width from the font size, first
    // we have to create the font.
    CClientDC dc(pCwnd);
    HFONT font;
	CFont *pOldFont;
    LOGFONT lf;
    XP_MEMSET(&lf,0,sizeof(LOGFONT));
    TEXTMETRIC tm;

    lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
    strcpy(lf.lfFaceName, IntlGetUIFixFaceName(0));
    lf.lfHeight = -MulDiv(9,dc.GetDeviceCaps(LOGPIXELSY), 72);
    lf.lfQuality = PROOF_QUALITY;    
	font = theApp.CreateAppFont( lf );

    pOldFont = dc.SelectObject(CFont::FromHandle(font));
    dc.GetTextMetrics(&tm);
    dc.SelectObject(pOldFont);

    cs.cx = (GetSystemMetrics(SM_CXFRAME)*2) + 
        (tm.tmAveCharWidth*(COMPOSE_WINDOW_WIDTH+(m_bUseHtml?2:0)));
    cs.cx += GetSystemMetrics(SM_CXVSCROLL);
    cs.cx += EDIT_MARGIN_OFFSET;
    
    // use 2/3 of the screen as the standard default height
    cs.cy = (GetSystemMetrics(SM_CYSCREEN)/3)*2;
	theApp.ReleaseAppFont(font);
    return rv;
}

void CComposeFrame::DisplayHeaders ( MSG_HEADER_SET headers )
{
   ASSERT(m_pComposeBar);
   m_pComposeBar->DisplayHeaders ( headers );
	m_pComposeBar->UpdateFixedSize ( );
   RecalcLayout ( );
#ifdef XP_WIN16
	m_pComposeBar->Invalidate();
#endif
   m_SavedHeaders = headers;
}

LONG CComposeFrame::OnToolController(UINT,LONG)
{
    if (m_bUseHtml)
	return (LONG)m_pToolBarController;
	return 0;
}

void CComposeFrame::SetModified(BOOL bValue)
{
    if (m_bUseHtml)
        EDT_SetDirtyFlag(GetMainContext()->GetContext(), bValue);
    else
        m_EditorParent.SetModified(bValue);
}

void CComposeFrame::OnSetFocus(CWnd * pOldWnd) 
{
    CGenericFrame::OnSetFocus(pOldWnd);
    if (GetFocusField())
        GetFocusField()->SetFocus();
}

BOOL CComposeFrame::CanCloseFrame(void)
{
    BOOL bModified = FALSE;
    if (m_bUseHtml){
        bModified = EDT_DirtyFlag(GetMainContext()->GetContext());
        MWContext *pMWContext;
        if( GetMainContext() == NULL ||
            (pMWContext = GetMainContext()->GetContext()) == NULL )
            return(FALSE);

        // Ignore close if in the process of doing something
        if( pMWContext->edit_saving_url ||
            pMWContext->waitingMode ||
            EDT_IsBlocked(pMWContext)  )
            return(FALSE);
    } else
        bModified = m_EditorParent.m_bModified;

	if (bModified) 
	{
	    CString csText, csTitle;
	    csText.LoadString(IDS_MESSAGENOTSENT);
	    csTitle.LoadString(IDS_MESSAGECOMPOSITION);
	    if(MessageBox( csText, csTitle, MB_YESNO | MB_ICONEXCLAMATION)==IDNO)
    		 return(FALSE);
	}
    return(TRUE);
    
}

void CComposeFrame::OnClose ( void )
{
	// See if the document will allow closing.
    
 	if (!CanCloseFrame())
		return;
    
    m_pComposeBar->Cleanup();
    
    if (m_bUseHtml){
        MWContext *pMWContext;
        if( GetMainContext() == NULL ||
            (pMWContext = GetMainContext()->GetContext()) == NULL )
            return;

        // Stop any active plugin
        if (!CheckAndCloseEditorPlugin(pMWContext)) 
            return;
        EDT_DestroyEditBuffer(pMWContext);
    }

    // Save the window position... as long as it is not iconified or
    // maximized... 
   if ( !IsIconic() && !IsZoomed() )
   {
      CRect rect;
	   GetWindowRect ( &rect );
	   PREF_SetRectPref("mail.compose_window_rect", 
         (int16)rect.left, (int16)rect.top, (int16)rect.right, (int16)rect.bottom);
	}
	    
   CWinCX * pWinCX = (CWinCX*)GetMainContext();
   if ( pWinCX )
   {
	   // close the context and stuff
	   CGenericView * pView = pWinCX->GetView();
	   if ( pView ) 
	      pView->FrameClosing ( );
   }
  
	m_pChrome->SaveToolbarConfiguration(ID_COMPOSE_MESSAGE_TOOLBAR, CString("Message_Toolbar"));
#ifdef XP_WIN32
    // Save toolbar state
   CFrameWnd::SaveBarState(ToolBarProfileName);
#endif
   CGenericFrame::OnClose();
}

// Used by the template to set the context type.

void CComposeFrame::SetType(MWContextType type)
{
    MWContext * xpContext = GetMainContext()->GetContext();
    ASSERT(xpContext);
    xpContext->type = type;
}

#ifdef XP_WIN32
int16 CComposeFrame::GetTitleWinCSID()	// jliu - used by genframe.cpp
{
	return INTL_DocToWinCharSetID( m_iCSID );
}
#endif

void CComposeFrame::SetCSID(int16 iCSID)
{
   if(iCSID != m_iCSID)
   {	
	m_iCSID = iCSID;
    m_pComposeBar->SetCSID(iCSID);
	if (m_cfTextFont) {
		theApp.ReleaseAppFont(m_cfTextFont);
	}

	CClientDC dc(this);
	LOGFONT lf;                 
	XP_MEMSET(&lf,0,sizeof(LOGFONT));
	lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
	strcpy(lf.lfFaceName, IntlGetUIFixFaceName(m_iCSID));
	lf.lfCharSet = IntlGetLfCharset(m_iCSID); 
	lf.lfHeight = -MulDiv(9,dc.GetDeviceCaps(LOGPIXELSY), 72);
	lf.lfQuality = PROOF_QUALITY;    
	m_cfTextFont = theApp.CreateAppFont( lf );
    TEXTMETRIC tm;
    CFont * pOld = dc.SelectObject(CFont::FromHandle(m_cfTextFont));
    dc.GetTextMetrics(&tm);
    dc.SelectObject(pOld);
    m_cxChar = tm.tmAveCharWidth;

	if (!m_bUseHtml)
		::SendMessage(m_Editor.GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfTextFont, FALSE);
	else
		EDT_SetEncoding(GetMainContext()->GetContext(), m_iCSID);

#ifdef XP_WIN32
	// jliu added the following to support CJK caption print
	SendMessage( WM_SETTINGCHANGE );
#endif

	RecalcLayout();
   }
}

// OnUpdateThis() enables/disables UI menus/tools based on the state
// which the backend is reporting.  If the menu is a check-item, that
// will be reflected into the appropriate item as well.

void CComposeFrame::OnUpdateThis ( CCmdUI* pCmdUI, MSG_CommandType tMenuType )
{
   if (!m_pComposePane || !pCmdUI)
      return;

	if (GetMsgPane() && MSG_DeliveryInProgress(GetMsgPane())) 
   {
      pCmdUI->Enable(FALSE);
      return;
   }

   XP_Bool bSelectable = FALSE;                    // is this a selectable command?
   XP_Bool bPlural = FALSE;                        // indicates whether to append "s" (which we never do)
   MSG_COMMAND_CHECK_STATE sState = MSG_Unchecked; // checked or unchecked 

    // call the backend requesting various state information
	MSG_CommandStatus( 
	m_pComposePane, // message pane
		tMenuType,      // MSG_CommandType value
		NULL,           
	0,
		&bSelectable,   // enable/disable
		&sState,        // checked/unchecked
		NULL,
		&bPlural);      // plural command?

    pCmdUI->Enable(bSelectable);                // enable or disable item
    pCmdUI->SetCheck(sState == MSG_Checked);    // set the check state

}

void CComposeFrame::MessageCommand(MSG_CommandType msgCmd)
{
    ASSERT(m_pComposePane);
    MSG_Command(m_pComposePane, msgCmd, NULL, 0);
}

void CComposeFrame::OnWrapLongLines ( )
{
   if (!m_bUseHtml)
   {
      int bHasFocus = 0;
      m_bWrapLongLines ^= 1;
      CRect rect;
      CString cs;

      if (GetFocus() == (CWnd *)&m_Editor)
          bHasFocus = 1;

      m_Editor.GetWindowRect(rect);
      m_EditorParent.ScreenToClient(rect);
      m_Editor.GetWindowText(cs);
      CFont *pFont = m_Editor.GetFont();

      m_Editor.DestroyWindow();
      m_Editor.Create ( 
          WS_VSCROLL | ES_MULTILINE | WS_CHILD | WS_VISIBLE | 
              ES_AUTOVSCROLL | ES_WANTRETURN | (m_bWrapLongLines ? 0 : ES_AUTOHSCROLL|WS_HSCROLL),
          rect, &m_EditorParent, IDC_COMPOSEEDITOR);
      
	  m_Editor.SetFont(pFont);
      m_Editor.SetWindowText(cs);

      if (bHasFocus)
          m_Editor.SetFocus();
   } 
}

void CComposeFrame::OnUpdateWrapLongLines ( CCmdUI * pCmdUI )
{
   OnUpdateButtonGeneral(pCmdUI);
   pCmdUI->SetCheck(m_bWrapLongLines == 1);
}

void CComposeFrame::OnNew ( )
{
    MSG_Command(GetMsgPane(), MSG_MailNew, NULL, 0);
}

void CComposeFrame::OnSelectAll()
{
    if (m_bUseHtml)
    	EDT_SelectAll(GetMainContext()->GetContext());
    else
    {
    	m_Editor.SetSel(0,-1);
    	m_Editor.SetFocus();
    }
}

void CComposeFrame::OnUpdateSelectAll(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

void CComposeFrame::OnUpdatePasteQuote(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(
		IsClipboardFormatAvailable(CF_TEXT)
#ifdef XP_WIN32
			|| IsClipboardFormatAvailable(CF_UNICODETEXT)
#endif
	);
}

void CComposeFrame::OnPasteQuote( )
{       
    char *lpszText;
    HANDLE hszText;
    char *lpszQuotedText = NULL;
    OpenClipboard();

#ifdef XP_WIN32	// ftang
	BOOL bDoneByUnicode = FALSE;
	// Try to use CF_UNICODETEXT first
    if ((CS_USER_DEFINED_ENCODING != (m_iCSID & ~CS_AUTO)) && 
		IsClipboardFormatAvailable(CF_UNICODETEXT) &&
		(hszText = (HANDLE) GetClipboardData(CF_UNICODETEXT)) )
    {
		char* lpszConvertedText = NULL;
    	lpszText = (char *) GlobalLock(hszText);

		// Now, let's convert the Unicode text into the datacsid encoding
		int ucs2len = CASTINT(INTL_UnicodeLen((INTL_Unicode*)lpszText));
		int	mbbufneeded = CASTINT(INTL_UnicodeToStrLen((m_iCSID & ~CS_AUTO), 
														(INTL_Unicode*)lpszText, 
														ucs2len));
		if(NULL != (lpszConvertedText = (char*)XP_ALLOC(mbbufneeded + 1)))
		{
			INTL_UnicodeToStr(m_iCSID, (INTL_Unicode*)lpszText, ucs2len, 
										(unsigned char*) lpszConvertedText, mbbufneeded + 1);
			
			if (m_bUseHtml) {
				MSG_PastePlaintextQuotation(GetMsgPane(), lpszConvertedText);
			} else {
				lpszQuotedText = MSG_ConvertToQuotation(lpszConvertedText);
				FE_InsertMessageCompositionText(GetMainContext()->GetContext(),
												lpszQuotedText, FALSE);
			}
			XP_FREEIF(lpszConvertedText);

			bDoneByUnicode = TRUE;
		}
    	GlobalUnlock(hszText);
    }
	
	if(bDoneByUnicode)
	{
		// Already done. Do nothing
	}
	else // not done by unicode try CF_TEXT now
#endif
    if (hszText = (HANDLE) GetClipboardData(CF_TEXT)) 
    {
    	lpszText = (char *) GlobalLock(hszText);
		if (m_bUseHtml) {
			MSG_PastePlaintextQuotation(GetMsgPane(), lpszText);
		} else {
			lpszQuotedText = MSG_ConvertToQuotation(lpszText);
			FE_InsertMessageCompositionText(GetMainContext()->GetContext(),
											lpszQuotedText, FALSE);
		}
    	GlobalUnlock(hszText);
    }
	if (lpszQuotedText) XP_FREE(lpszQuotedText);
    CloseClipboard();
}

// UpdateToolbar() changes the command IDs as well as the assigned bitmap
// to reflect the current state.. deferred send or immediate delivery.
void CComposeFrame::UpdateToolBar(void)
{
    ApiToolBar(pIToolBar,m_pChrome);

	if ( pIToolBar ) 
    {
        pIToolBar->SetButtons(ComposeCodes, sizeof(ComposeCodes) / sizeof(UINT));
        // this is currently loading one bitmap.. this needs to be fixed to add
        // the deferred bitmaps as well.
        pIToolBar->LoadBitmap(MAKEINTRESOURCE(IDB_COMPOSETOOLBAR));
	}
}

void CComposeFrame::OnDoneGoingOffline()
{
    BOOL bGlobalDeferred = FALSE;
	PREF_GetBoolPref("network.online", &bGlobalDeferred);
	bSendDeferred = !bGlobalDeferred;
  	LPNSTOOLBAR pIToolBar;
	m_pChrome->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );

	if ( pIToolBar ) 
    {
        CString statusStr, toolTipStr, textStr;
        if (bSendDeferred)
        {
            WFE_ParseButtonString(IDM_SENDLATER, statusStr, toolTipStr, textStr);
		    CCommandToolbar *pToolBar = (CCommandToolbar *) CWnd::FromHandlePermanent( pIToolBar->GetHWnd() );
            if ( pToolBar)
            {
                CToolbarButton *pBarButton = pToolBar->GetNthButton(0);
                if (pBarButton)
                {
 		            pToolBar->ReplaceButtonBitmapIndex(IDM_SENDNOW, 7);
                    pBarButton->SetToolTipText(toolTipStr);
                    pBarButton->SetStatusText(statusStr);
                    pBarButton->SetText(textStr);
                    pBarButton->SetButtonCommand(IDM_SENDLATER);
                }
		        pIToolBar->Release();
            }
        }
        else
        {
		    WFE_ParseButtonString(IDM_SENDNOW, statusStr, toolTipStr, textStr);
		    CCommandToolbar *pToolBar = (CCommandToolbar *) CWnd::FromHandlePermanent( pIToolBar->GetHWnd() );
            if (pToolBar)
            {
                CToolbarButton *pBarButton = pToolBar->GetNthButton(0);
 	            pToolBar->ReplaceButtonBitmapIndex(IDM_SENDLATER, 0);
                if (pBarButton)
                {
                    pBarButton->SetToolTipText(toolTipStr);
                    pBarButton->SetStatusText(statusStr);
                    pBarButton->SetText(textStr);
                    pBarButton->SetButtonCommand(IDM_SENDNOW);
                }
                pIToolBar->Release();
            }
        }
    }
}

void CComposeFrame::OnUpdateButtonGeneral(CCmdUI *pCmdUI)    
{ 
	OnUpdateThis(pCmdUI, MSG_SendMessage );
}

void CComposeFrame::OnUpdateSend(CCmdUI *pCmdUI)
{
	OnUpdateThis(pCmdUI, bSendDeferred ? MSG_SendMessageLater : MSG_SendMessage);
	pCmdUI->SetText(bSendDeferred ? szLoadString(IDS_DEFAULTSENDNOW) : szLoadString(IDS_DEFAULTSENDLATER));
}

void CComposeFrame::OnUpdateSendNow(CCmdUI *pCmdUI)
{

	OnUpdateThis(pCmdUI, MSG_SendMessage);
	pCmdUI->SetText(bSendDeferred ? szLoadString(IDS_SENDNOW) : szLoadString(IDS_DEFAULTSENDNOW));
}

void CComposeFrame::OnUpdateSendLater(CCmdUI *pCmdUI)
{

	OnUpdateThis(pCmdUI, MSG_SendMessageLater);
	pCmdUI->SetText(bSendDeferred ? szLoadString(IDS_DEFAULTSENDLATER) : szLoadString(IDS_SENDLATER));
}

// This function is send to the backend for a callback when quoting
// text. Currently, it simply calls the pre-existing insert code.
int WFE_InsertMessage(void *closure, const char * data)
{
   if (data) {
      FE_InsertMessageCompositionText((MWContext*)closure,data,FALSE);
   }
   else {
	   MSG_Pane *pPane = MSG_FindPane((MWContext *)closure, MSG_COMPOSITIONPANE);
	   if (pPane) {
			CComposeFrame * pCompose = (CComposeFrame *) MSG_GetFEData(pPane);
			if (pCompose->GetQuoteSel() != -1) {
				if (! pCompose->UseHtml()) {
					int32 eReplyOnTop = 0;
					int start = ((int) pCompose->GetQuoteSel()), end = start;
					int tmpStart = 0;

					if (PREF_NOERROR ==
							PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop)) {
						switch(eReplyOnTop) {
						case 1:
						default:
							pCompose->GetEditor()->SetSel(start, end);
							break;
						case 2:
							pCompose->GetEditor()->GetSel(tmpStart, end);
							pCompose->GetEditor()->SetSel(start, end);
							break;
						case 3:
							pCompose->GetEditor()->GetSel(tmpStart, end);
							pCompose->GetEditor()->SetSel(end, start);
							break;
						}
					}
				}
				pCompose->SetQuoteSel(-1);
			}
	   }
   }
   return 0;
}

void CComposeFrame::OnQuoteOriginal( void )
{
	SetQuoteSelection();
    MSG_QuoteMessage(GetMsgPane(),WFE_InsertMessage,(void*)GetMainContext()->GetContext());
}

void CComposeFrame::OnAddressTab(void)
{
    m_pComposeBar->OnAddressTab();
}

void CComposeFrame::OnAttachTab(void)
{
    m_pComposeBar->OnAttachTab();
}

void CComposeFrame::OnOptionsTab(void)
{
    m_pComposeBar->OnOptionsTab();
}

void CComposeFrame::OnCollapse(void)
{
    m_pComposeBar->OnCollapse();
}

// OnSaveAs().. This should probably have better backend support.  This function
// gets the current message body text and saves it to a user specified location.

void CComposeFrame::OnSaveAs(void)
{
    // Call the file-picker to get a filename/location.
    char * pPath = wfe_GetSaveFileName(m_hWnd, XP_GetString(MK_MSG_SAVE_AS), szLoadString(IDS_DEFAULTSAVEAS), 0);
    
    // if a valid path was specified, continue with save
	if (pPath && strlen(pPath)) 
    {
		uint32 ulBodySize;
		char *pBody;
	
	// open the file for writing
		FILE *pFile = fopen(pPath, "w");
	
	// if the file was opened successfully, continue with save
		if (pFile)
	{
			FE_GetMessageBody(m_pComposePane, &pBody, &ulBodySize, NULL);
			if (!fwrite(pBody, sizeof(char), CASTSIZE_T(ulBodySize), pFile)) 
	    {
		// couldn't write file
				char szOpenError[512];
				sprintf(szOpenError, szLoadString(IDS_CANTWRITETOFILE), pPath);
				FE_Alert(GetMainContext()->GetContext(), szOpenError);
			}
			if (UseHtml())
				XP_HUGE_FREE(pBody);
			else
				XP_FREE(pBody); 
			fclose(pFile);
		} 
	else 
	{
	    // can't open file
			char szOpenError[512];
			sprintf(szOpenError, szLoadString(IDS_CANTWRITETOFILE), pPath);
			FE_Alert(GetMainContext()->GetContext(), szOpenError);
		}
		XP_FREE(pPath);
	}
}


void CComposeFrame::OnCheckSpelling()
{
   CWnd * pWnd = GetEditorWnd();
   if (pWnd)
      pWnd->SendMessage(WM_COMMAND,ID_CHECK_SPELLING);
}

// OnSaveAsDraft() sends based on the current delivery state.

void CComposeFrame::OnSaveDraft ( void )

{
    ASSERT(m_pComposeBar);
    m_pComposeBar->UpdateHeaderInfo ( );

    // first we must get the text from the front end and then
    // tell the backend what text to send.

	uint32 ulBodySize;
	char *pBody;

	FE_GetMessageBody(GetMsgPane(), &pBody, &ulBodySize, NULL);
    
    // if we could get text, tell the backend
    if (ulBodySize) 
	MSG_SetCompBody(GetMsgPane(),pBody);

    // start the meteors tumbling    
	GetChrome()->StartAnimation();

	MWContext *context;
	context = GetMainContext()->GetContext();

    // Pass current csid to backend, so it can do right conversion for 
    // 8bit header.    
	INTL_SetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context), m_iCSID);

    MSG_Command(GetMsgPane(), MSG_SaveDraft, 0, 0);
	
	GetChrome()->StopAnimation();

    if (!m_bUseHtml)
        m_EditorParent.m_bModified = FALSE;

}



void CComposeFrame::OnUpdateSaveDraft(CCmdUI *pCmdUI)
{

    // Change the string based on the deferred sending status

    OnUpdateThis(pCmdUI, MSG_SaveDraft );

}


void CComposeFrame::OnSaveTemplate ( void )

{
    ASSERT(m_pComposeBar);
    m_pComposeBar->UpdateHeaderInfo ( );

    // first we must get the text from the front end and then
    // tell the backend what text to send.

	uint32 ulBodySize;
	char *pBody;

	FE_GetMessageBody(GetMsgPane(), &pBody, &ulBodySize, NULL);
    
    // if we could get text, tell the backend
    if (ulBodySize) 
	MSG_SetCompBody(GetMsgPane(),pBody);

    // start the meteors tumbling    
	GetChrome()->StartAnimation();

	MWContext *context;
	context = GetMainContext()->GetContext();

    // Pass current csid to backend, so it can do right conversion for 
    // 8bit header.    
	INTL_SetCSIDocCSID(LO_GetDocumentCharacterSetInfo(context), m_iCSID);

    MSG_Command(GetMsgPane(), MSG_SaveTemplate, 0, 0);
	
	GetChrome()->StopAnimation();

    if (!m_bUseHtml)
        m_EditorParent.m_bModified = FALSE;
}

void CComposeFrame::OnUpdateSaveTemplate(CCmdUI *pCmdUI)
{

    // Change the string based on the deferred sending status

    OnUpdateThis(pCmdUI, MSG_SaveTemplate );

}

LRESULT CComposeFrame::OnButtonMenuOpen(WPARAM wParam, LPARAM lParam)
{
	HMENU hMenu = (HMENU) lParam;
	UINT nCommand = (UINT) LOWORD(wParam);
    if (nCommand == IDM_ATTACHFILE){
        ::AppendMenu(hMenu,MF_STRING|MF_ENABLED, IDM_ATTACHFILE, szLoadString(IDS_TOOLATTACHFILE));
        ::AppendMenu(hMenu,MF_STRING|MF_ENABLED, IDM_ATTACHWEBPAGE, szLoadString(IDS_ATTACHWEBPAGE));
        ::AppendMenu(hMenu,MF_SEPARATOR, 0, NULL);
        ::AppendMenu(hMenu,MF_STRING|MF_ENABLED, IDM_ATTACHMYCARD, szLoadString(IDS_ATTACHMYCARD));
	    return 1;
    } else if (GetMainWinContext()){
        // Route other messages to Edit View, which handles messages common to all Composer frames
        return ::SendMessage( GetMainWinContext()->GetPane(), NSBUTTONMENUOPEN, wParam, lParam);
    }
    return 0;
}

// DoSend() sends based on the current delivery state.
void CComposeFrame::DoSend ( BOOL bNow )
{
    ASSERT(m_pComposeBar);
    LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
    if (pIAddress)
    {
        CListBox * pListBox = pIAddress->GetListBox();
        if (::IsWindow(pListBox->m_hWnd))
	        pListBox->SendMessage(WM_NOTIFYSELECTIONCHANGE);
    }
	MSG_ClearComposeHeaders(GetMsgPane());
    m_pComposeBar->UpdateHeaderInfo ( );

    // first we must get the text from the front end and then
    // tell the backend what text to send.
	uint32 ulBodySize;
	char *pBody = NULL;
	FE_GetMessageBody(GetMsgPane(), &pBody, &ulBodySize, NULL);
    
#ifndef _WIN32
	if (ulBodySize > 32767) {
		MessageBox(szLoadString(IDS_MSGTOBIG), szLoadString(IDS_TITLE_ERROR), MB_ICONSTOP|MB_OK);
		return;
	}
#endif
    // if we could get text, tell the backend
    if (pBody) 
    	MSG_SetCompBody(GetMsgPane(),pBody);

    int errcode = 0;
    MWContext * context = GetMainContext()->GetContext();
    while ((errcode = MSG_SanityCheck(GetMsgPane(),errcode))!=0)
    {
    	GetChrome()->StopAnimation();
        if (errcode == MK_MSG_MISSING_SUBJECT)
        {
            char * ptr = PromptMessageSubject();
            if (ptr != NULL)
            {
    	        MSG_SetCompHeader(GetMsgPane(),MSG_SUBJECT_HEADER_MASK,ptr);
                XP_FREE(ptr);
            }
            else
                return;
        }
        else if (!FE_Confirm(context,XP_GetString(errcode)))
            return;
    }
    
    // Pass current csid to backend, so it can do right conversion for 
    // 8bit header.  
	INTL_SetCSIWinCSID(LO_GetDocumentCharacterSetInfo(context), m_iCSID);
   
    MSG_Command(GetMsgPane(), bNow ? MSG_SendMessage : MSG_SendMessageLater, 0, 0);

	if (GetMsgPane() && MSG_DeliveryInProgress(GetMsgPane())) {
		// start the meteors tumbling    
		GetChrome()->StartAnimation();
		LPNSSTATUSBAR pIStatusBar = NULL;
		GetChrome()->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
		if ( pIStatusBar ) {
			pIStatusBar->ModalStatus( TRUE, 0, szLoadString(IDS_SENDINGMESSAGE));
			pIStatusBar->Release();
		}
	}
  // rhp - this condition is for MAPI UI-less send operations...
  else
  {
    if (!IsWindowVisible())       // First, check if the window is not already visible
      ShowWindow(SW_SHOW);        // and if not, get it on the screen for the user to deal with...    
  }
  // rhp 
}

void CComposeFrame::OnAttachMyCard()
{
    ASSERT(m_pComposeBar);
    m_pComposeBar->SetAttachMyCard(!m_pComposeBar->GetAttachMyCard());
}

void CComposeFrame::OnUpdateAttachMyCard(CCmdUI * pCmdUI)
{
    pCmdUI->Enable(TRUE);
    pCmdUI->SetCheck(m_pComposeBar->GetAttachMyCard());
}

void CComposeFrame::OnAttachFile()
{
    ASSERT(m_pComposeBar);
    m_pComposeBar->OnAttachTab();
    m_pComposeBar->UpdateWindow();
    m_pComposeBar->AttachFile();
}
 
void CComposeFrame::OnAttachUrl()
{
    ASSERT(m_pComposeBar);
    m_pComposeBar->OnAttachTab();
    m_pComposeBar->UpdateWindow();
    m_pComposeBar->AttachUrl();
}

void CComposeFrame::ConvertToPlainText()
{
   if (MessageBox(szLoadString(IDS_CONVERTWARN), szLoadString(IDS_CONVERTTITLE), 
      MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
   {

      MWContext *pMWContext;
      if( GetMainContext() == NULL ||
         (pMWContext = GetMainContext()->GetContext()) == NULL )
             return;
        // Stop any active plugin
      if (!CheckAndCloseEditorPlugin(pMWContext)) 
         return;
      EDT_DestroyEditBuffer(pMWContext);
      pMWContext->is_editor = FALSE;
      m_pChrome->SetMenu(IDR_COMPOSEPLAIN);
      DestroyHtmlToolbars();
      RecalcLayout();
      CreatePlainTextEditor();
	  ::SendMessage(m_Editor.GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfTextFont, FALSE);
      m_bUseHtml = FALSE;
   }
}

void CComposeFrame::ConvertToHtml()
{
   m_pChrome->SetMenu(IDR_COMPOSEFRAME);
   CreateHtmlToolbars();
   ShowHtmlToolbars();
   RecalcLayout();
   SetEditorParent(NULL);
   m_Editor.DestroyWindow();
   m_EditorParent.DestroyWindow();
   m_bUseHtml = TRUE;

   if (GetMainContext() != NULL)
   {
      CWinCX * pWinCX = GetMainContext()->IsFrameContext() ? 
         VOID2CX(GetMainContext(),CWinCX) : NULL;
      if (pWinCX != NULL)
      {
         MWContext * pContext = pWinCX->GetContext();
         if (!pContext->is_editor)
         {
            pContext->is_editor = TRUE;
            URL_Struct * pUrl = NET_CreateURLStruct(EDT_NEW_DOC_URL,NET_DONT_RELOAD);
            if (pUrl != NULL)
            {
               pWinCX->GetUrl(pUrl, FO_CACHE_AND_PRESENT);
               pWinCX->GetContext()->bIsComposeWindow = TRUE;
            }
         }  
      }
   }
}

void CComposeFrame::OnConvert()
{
   if (m_bUseHtml)   
      ConvertToPlainText();
   else
      ConvertToHtml();   
}

void CComposeFrame::OnUpdateConvert(CCmdUI * pCmdUI)
{
   pCmdUI->Enable(FALSE);   // wait for it
}

void CComposeFrame::OnSend()
{
	DoSend(!bSendDeferred);
}

void CComposeFrame::OnSendNow()
{
   DoSend(TRUE );
}

void CComposeFrame::OnSendLater()
{
   DoSend(FALSE);
}

void CComposeFrame::MakeComposeBarVisible()
{
    if (!GetComposeBar()->IsVisible())
		GetComposeBar()->OnToggleShow();
	else if (GetComposeBar()->IsCollapsed())
	{
		GetComposeBar()->OnCollapse();
        CCustToolbar * pToolbar = GetChrome()->GetCustomizableToolbar();
        if( pToolbar ) 
            pToolbar->RemoveExternalTab(1);
       	RecalcLayout();
	}
}

void CComposeFrame::OnViewAddresses()
{
	MakeComposeBarVisible();
    GetComposeBar()->SendMessage(WM_COMMAND,IDC_ADDRESSTAB);
}

void CComposeFrame::OnViewAttachments()
{
	MakeComposeBarVisible();
    m_pComposeBar->m_pSubjectEdit->SetFocus();
    UpdateWindow();
    GetComposeBar()->SendMessage(WM_COMMAND,IDC_ATTACHTAB);
}

void CComposeFrame::OnViewOptions()
{
	MakeComposeBarVisible();
    m_pComposeBar->m_pSubjectEdit->SetFocus();
    UpdateWindow();
    GetComposeBar()->PostMessage(WM_COMMAND,IDC_OPTIONSTAB);
}

void CComposeFrame::OnUpdateViewAddresses(CCmdUI * pCmdUI)
{
    pCmdUI->SetRadio(GetComposeBar()->GetTab() == IDX_COMPOSEADDRESS);
    pCmdUI->Enable(TRUE);
}

void CComposeFrame::OnUpdateViewAttachments(CCmdUI * pCmdUI)
{
    pCmdUI->SetRadio(GetComposeBar()->GetTab() == IDX_COMPOSEATTACH);
    pCmdUI->Enable(TRUE);
}

void CComposeFrame::OnUpdateViewOptions(CCmdUI * pCmdUI)
{
    pCmdUI->SetRadio(GetComposeBar()->GetTab() == IDX_COMPOSEOPTIONS);
    pCmdUI->Enable(TRUE);
}

// OnSetInitialFocus - Invoked after the frame window, toolbar and view are created.
// It sets the initial keyboard focus.

LONG CComposeFrame::OnSetInitialFocus(WPARAM wParam, LPARAM lParam)
{
    return 0;
}

void CComposeFrame::SetInitialText( const char *pText )
{
	if (m_pInitialText)
		XP_FREE (m_pInitialText);

	m_pInitialText = NULL;

	if (pText)
		m_pInitialText = XP_STRDUP(pText);
}

BOOL CComposeFrame::BccOnly(void)
{
    LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
    CListBox * pListBox = pIAddress->GetListBox();
    int count = pListBox->GetCount();
    int total = 0;
    for (int i=0; i<count; i++)
    {
        char * szName, * szType;
	    if(pIAddress->GetEntry(i,&szType, &szName))
		    if(strcmp(szType,szLoadString(IDS_ADDRESSBCC)))
			    total++;
    }
    return !total;
}

void CComposeFrame::InsertInitialText(void)
{
	const char *pBody = MSG_GetCompBody(GetMsgPane());

    LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
    if (pIAddress)
    {
        CListBox * pListBox = pIAddress->GetListBox();
        if (::IsWindow(pListBox->m_hWnd))
    		pListBox->ResetContent();
    }

	if (m_pInitialText && strlen(m_pInitialText))
	{
		EDT_ReadFromBuffer(GetMainContext()->GetContext(),
						m_pInitialText);
		XP_FREEIF(m_pInitialText);
	}
	else if (pBody && strlen(pBody))
	{
		FE_InsertMessageCompositionText( GetMainContext()->GetContext(), pBody, TRUE);
		FE_InsertMessageCompositionText( GetMainContext()->GetContext(), "\n", TRUE);
		MSG_SetCompBody(GetMsgPane(), "");
	}

	CompleteComposeInitialization();
    if (m_bUseHtml)
        EDT_SetDirtyFlag(GetMainContext()->GetContext(), FALSE);

    if (MSG_ShouldAutoQuote(GetMsgPane()))
    {
		SetQuoteSelection();
        MSG_QuoteMessage(GetMsgPane(),WFE_InsertMessage,(void*)GetMainContext()->GetContext());
    }
}

void CComposeFrame::CompleteComposeInitialization(void)
{
    MSG_HeaderEntry * entry;
	int nCount;
    nCount = MSG_RetrieveStandardHeaders(GetMsgPane(),&entry);
    LPADDRESSCONTROL pIAddress = m_pComposeBar->GetAddressWidgetInterface();
    CListBox * pListBox = pIAddress->GetListBox();
	if (nCount > 0) 
	{
		int i;
        pListBox->LockWindowUpdate();
		for (i=0; i<nCount; i++)
		{
            pIAddress->AppendEntry(FALSE,
	    	    MSG_HeaderMaskToString(entry[i].header_type),
                entry[i].header_value,
                0, 0);
			XP_FREE(entry[i].header_value);
		}
        pIAddress->AppendEntry();
        ::LockWindowUpdate(NULL);
	}

	if (entry)
		XP_FREE(entry);

	const char * subject = MSG_GetCompHeader(GetMsgPane(),MSG_SUBJECT_HEADER_MASK);

    if (!nCount)
    	pIAddress->AppendEntry(NULL);

    // in case of the existance hidden headers, get new count
    nCount = pListBox->GetCount();

    if (subject && strlen(subject))
    {
    	m_pComposeBar->m_pSubjectEdit->SetWindowText(subject);
        GetChrome()->SetDocumentTitle(subject);
    }

	if (nCount<=1 || BccOnly())  
    {
        if (BccOnly())
        {
            pIAddress->DeleteEntry(pListBox->GetCount()-1);
        	pIAddress->AppendEntry(TRUE, szLoadString(IDS_ADDRESSTO),"",0,0);
        }
        pListBox->UpdateWindow();
		pListBox->SetFocus();
        SetFocusField(pListBox);
    }
	else if (subject && strlen(subject))
    {
    	if (!UseHtml())
        {
	    	GetEditor()->SetFocus();
            SetFocusField(GetEditor());
        }
    }
	else
	{
		m_pComposeBar->m_pSubjectEdit->SetSel(-1,0);
		m_pComposeBar->m_pSubjectEdit->SetFocus();
        SetFocusField(m_pComposeBar->m_pSubjectEdit);
	}

   // Set the message priority based on the fileds
    const char * priorityString = MSG_GetCompHeader(GetMsgPane(), MSG_PRIORITY_HEADER_MASK);
	if (priorityString) 
	{
		// Backend deals only untranslated priority; we have to do some mapping here
		if (strstr(priorityString, "Highest"))
			m_pComposeBar->m_iPriorityIdx = MSG_HighestPriority - 2;
		else if (strstr(priorityString, "High"))
			m_pComposeBar->m_iPriorityIdx = MSG_HighPriority - 2;
		else if (strstr(priorityString, "Lowest"))
			m_pComposeBar->m_iPriorityIdx = MSG_LowestPriority - 2;
		else if (strstr(priorityString, "Low"))
			m_pComposeBar->m_iPriorityIdx = MSG_LowPriority - 2;
		else 
			m_pComposeBar->m_iPriorityIdx = MSG_NormalPriority - 2;
	}

	switch (MSG_GetHTMLAction(GetMsgPane())) {
		default:
		case MSG_HTMLAskUser:
			m_pComposeBar->m_pszMessageFormat = strdup(szLoadString(IDS_FORMAT_ASKME));
			break;
		case MSG_HTMLUseMultipartAlternative:
			m_pComposeBar->m_pszMessageFormat = strdup(szLoadString(IDS_FORMAT_BOTH));
			break;
		case MSG_HTMLConvertToPlaintext:
			m_pComposeBar->m_pszMessageFormat = strdup(szLoadString(IDS_FORMAT_PLAIN));
			break;
		case MSG_HTMLSendAsHTML:
			m_pComposeBar->m_pszMessageFormat = strdup(szLoadString(IDS_FORMAT_HTML));
			break;
	}

    m_pComposeBar->SetAttachMyCard(MSG_GetCompBoolHeader(GetMsgPane(),MSG_ATTACH_VCARD_BOOL_HEADER_MASK));
    m_pComposeBar->SetReturnReceipt(MSG_GetCompBoolHeader(GetMsgPane(),MSG_RETURN_RECEIPT_BOOL_HEADER_MASK));
    m_pComposeBar->SetUseUUENCODE(MSG_GetCompBoolHeader(GetMsgPane(),MSG_UUENCODE_BINARY_BOOL_HEADER_MASK));
    UpdateSecurityOptions();

    m_bInitialized = TRUE;
}

void CComposeFrame::UpdateSecurityOptions(void)
{
    HG27625
}

void CComposeFrame::GoldDoneLoading ()
{
   MSG_InitializeCompositionPane(GetMsgPane(),m_pOldContext,m_pFields);
   PostMessage(WM_COMP_SET_INITIAL_FOCUS);
   UpdateAttachmentInfo();
}

////////////////////////////////////////////////////////////////////////////////////
// Begin the pseudo-backend code here.

const char * MSG_HeaderMaskToString(MSG_HEADER_SET header)
{
    const char * retval = NULL;
    switch (header)
    {
	    case MSG_TO_HEADER_MASK:
	        retval = szLoadString(IDS_ADDRESSTO);
	        break;
	    case MSG_CC_HEADER_MASK:
	        retval = szLoadString(IDS_ADDRESSCC);
	        break;
	    case MSG_BCC_HEADER_MASK:
	        retval = szLoadString(IDS_ADDRESSBCC);
	        break;
	    case MSG_REPLY_TO_HEADER_MASK:
	        retval = szLoadString(IDS_ADDRESSREPLYTO);
	        break;
	    case MSG_NEWSGROUPS_HEADER_MASK:
	        retval = szLoadString(IDS_ADDRESSNEWSGROUP);
	        break;
	    case MSG_FOLLOWUP_TO_HEADER_MASK:
	        retval = szLoadString(IDS_ADDRESSFOLLOWUPTO);
	        break;
	    default:
	        XP_ASSERT(0);
    }
    return retval;
}

MSG_HEADER_SET MSG_StringToHeaderMask(char * string)
{
    MSG_HEADER_SET retval = 0;
    if (!stricmp(szLoadString(IDS_ADDRESSTO),string))
        retval = MSG_TO_HEADER_MASK;
    else if (!stricmp(szLoadString(IDS_ADDRESSCC),string))
        retval = MSG_CC_HEADER_MASK;
    else if (!stricmp(szLoadString(IDS_ADDRESSBCC),string))
        retval = MSG_BCC_HEADER_MASK;
    else if (!stricmp(szLoadString(IDS_ADDRESSREPLYTO),string))
        retval = MSG_REPLY_TO_HEADER_MASK;
    else if (!stricmp(szLoadString(IDS_ADDRESSNEWSGROUP),string))
        retval = MSG_NEWSGROUPS_HEADER_MASK;
    else if (!stricmp(szLoadString(IDS_ADDRESSFOLLOWUPTO),string))
        retval = MSG_FOLLOWUP_TO_HEADER_MASK;
    else
        XP_ASSERT(0);
    return retval;
}

BOOL CComposeFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
   BOOL bRetVal = CGenericFrame::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
   if (nCode == CN_UPDATE_COMMAND_UI)
      if (!bRetVal && !m_bUseHtml)
         bRetVal = m_Editor.OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
   return bRetVal;
}

void CComposeFrame::OnToggleMessageToolbar()
{
	GetChrome()->ShowToolbar(ID_COMPOSE_MESSAGE_TOOLBAR, !GetChrome()->GetToolbarVisible(ID_COMPOSE_MESSAGE_TOOLBAR));
}

void CComposeFrame::OnUpdateToggleMessageToolbar(CCmdUI* pCmdUI)
{
   pCmdUI->SetCheck(GetChrome()->GetToolbarVisible(ID_COMPOSE_MESSAGE_TOOLBAR));
   pCmdUI->Enable(TRUE);
}

void CComposeFrame::OnToggleAddressArea()
{
    m_pComposeBar->OnToggleShow();
}

void CComposeFrame::OnUpdateToggleAddressArea(CCmdUI * pCmdUI)
{
   pCmdUI->SetCheck(m_pComposeBar->IsVisible());
   pCmdUI->Enable(TRUE);
}

#define UNSECURE_INDEX		9
#define SECURE_INDEX		11
#define UNSEC_SIGNED_INDEX	8
#define SEC_SIGNED_INDEX	10

void CComposeFrame::OnUpdateSecurity(CCmdUI * pCmdUI)
{
	HG72521
   OnUpdateButtonGeneral(pCmdUI);
}

void CComposeFrame::OnUpdateSecureStatus(CCmdUI *pCmdUI)
{
	HG87282
}

void CComposeFrame::OnUpdateShowNamePicker(CCmdUI *pCmdUI)
{
#ifdef MOZ_NEWADDR
	pCmdUI->Enable(FALSE);

    LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
    if (pIAddress)
    {
		CEdit *pEdit = pIAddress->GetAddressNameField();

		if(GetFocus() == pEdit)
		{
			pCmdUI->Enable(TRUE);
		}
    }

#else
	pCmdUI->Enable(FALSE);
#endif
}

void CComposeFrame::OnShowNamePicker()
{
#ifdef MOZ_NEWADDR

	//need to get text of current selection
    LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
    if (pIAddress)
    {
		pIAddress->ShowNameCompletionPicker(this);
    }


#endif
}

void CComposeFrame::OnUpdateToggleNameCompletion(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(FALSE);

#ifdef MOZ_NEWADDR

	LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
	if(pIAddress)
	{
		pCmdUI->Enable(TRUE);
		BOOL bHasNameCompletion = pIAddress->GetEntryHasNameCompletion();

		if(bHasNameCompletion)
		{
			pCmdUI->SetText(::szLoadString(IDS_NAMECOMPLETIONOFF));

		}
		else
		{
			pCmdUI->SetText(::szLoadString(IDS_NAMECOMPLETIONON));
		}

	}
#endif

}

void CComposeFrame::OnToggleNameCompletion()
{
#ifdef MOZ_NEWADDR
	LPADDRESSCONTROL pIAddress = GetAddressWidgetInterface();
	if(pIAddress)
	{
		BOOL bHasNameCompletion = pIAddress->GetEntryHasNameCompletion();

		pIAddress->SetEntryHasNameCompletion(!bHasNameCompletion);
	}
#endif
}


char * CComposeFrame::PromptMessageSubject()
{
    CWnd * pWnd = GetFocus();
    CString csText;
    CString csDefault;
    csText.LoadString(IDS_COMPOSE_NOSUBJECT);
    csDefault.LoadString(IDS_COMPOSE_DEFAULTNOSUBJECT);
    MWContext * pContext = GetMainContext()->GetContext();
    char * ptr = ABSTRACTCX(pContext)->Prompt(pContext, csText, csDefault);
    pWnd->SetFocus();
    return ptr;
}

void CComposeFrame::OnSecurity()
{
    m_pComposeBar->UpdateHeaderInfo ( );
    CGenericFrame::OnSecurity();
}

void CComposeFrame::RefreshNewEncoding(int16 doc_csid, BOOL bSave )
{
	int16 win_csid = INTL_DocToWinCharSetID(doc_csid);
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo( GetMainContext()->GetContext() );

	INTL_SetCSIDocCSID( c, doc_csid );	
	INTL_SetCSIWinCSID( c, win_csid );
	
	SetCSID( doc_csid );
}

LRESULT CComposeFrame::OnFindReplace(WPARAM wParam, LPARAM lParam) 
{
   if (!m_bUseHtml)
   {
      return m_Editor.OnFindReplace(wParam,lParam);
   }
   return CGenericFrame::OnFindReplace(wParam,lParam);
}

void CComposeFrame::GetMessageString(UINT MenuID, CString& Message) const
{
#ifdef EDITOR
    // static function for both CEditFrame and CComposeFrame
    //  to get runtime-determined statusline strings for menus
    if( edt_GetMessageString(GetActiveView(), MenuID, Message) )
        return;
#endif

    CGenericFrame::GetMessageString(MenuID, Message);
}

// OnSetMessageString - Override of CGenericFrame's WM_SETMESSAGESTRING message handler. We need this 
// because the base class is supressing the dynamically-created menu help strings for Composer
LRESULT CComposeFrame::OnSetMessageString(WPARAM wParam, LPARAM lParam)
{
	if( edt_IsEditorDynamicMenu(wParam) )
		return CFrameWnd::OnSetMessageString(wParam, lParam);
    else
		return CGenericFrame::OnSetMessageString(wParam, lParam);
}

// rhp - Added for sending the message when the window is not shown
// This is to wait until attachments are loaded for MAPI Operations
void CComposeFrame::UpdateComposeWindowForMAPI(void) 
{ 
    TRACE("UPDATE WINDOW FOR MAPI\n"); 
    if (GetMAPISendMode() == MAPI_SEND)
    {
      TRACE("SEND WINDOW FOR MAPI\n"); 
      PostMessage(WM_COMMAND, IDM_SEND); 
      SetMAPISendMode(MAPI_IGNORE);
    }
    else if (GetMAPISendMode() == MAPI_SAVE)
    {
      TRACE("SAVE WINDOW FOR MAPI\n"); 
      PostMessage(WM_COMMAND, IDM_SAVEASDRAFT); 
      SetMAPISendMode(MAPI_QUITWHENDONE);
    }
    else if (GetMAPISendMode() == MAPI_QUITWHENDONE)
    {
      PostMessage(WM_CLOSE); 
      SetMAPISendMode(MAPI_IGNORE);
    }
} 

// rhp - This is to respond to a custom message to identify compose windows
LONG CComposeFrame::OnProcessMAPIMessage(WPARAM wParam, LPARAM lParam)
{
  return(MAPI_CUSTOM_RET_CODE);
}
