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

// netsvw.cpp : implementation of the CNetscapeView class
//

#include "stdafx.h"

#include "dialog.h"
#include "cntritem.h"
#include "cxsave.h"
#include "cxprint.h"
#include "netsprnt.h"
#include "edt.h"
#include "netsvw.h"
#include "mainfrm.h"
#include "prefapi.h"
#ifdef EDITOR
#include "shcut.h"
#include "edframe.h"
#include "pa_tags.h"   // Needed for P_MAX
#endif
#include "libevent.h"
#ifdef XP_WIN32
#include "shlobj.h" // For clipboard formats
#endif
#include "np.h"

#include "intl_csi.h"
#include "libi18n.h"

#ifdef MOZ_MAIL_NEWS
// For hiding/restoring Mail Composer toolbars 
//  during Print Preview
// TODO: This should be changed to go through chrome mechanism;
//       CNetscapeView shouldn't know about CComposeFrame
#include "compfrm.h"
#include "compbar.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
#ifdef EDITOR
#define GET_MWCONTEXT  (GetContext() == NULL ? NULL : GetContext()->GetContext())

// Used to suppress interaction during URL loading, editor setup, layout, etc.
#define CAN_INTERACT  (GetContext() != NULL && GetContext()->GetContext() != NULL \
                       && !GetContext()->GetContext()->waitingMode )

extern char *EDT_NEW_DOC_URL;
extern char *EDT_NEW_DOC_NAME;
#endif
// CNetscapeView

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CNetscapeView, CGenericView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CNetscapeView, CGenericView)
    //{{AFX_MSG_MAP(CNetscapeView)
    ON_WM_RBUTTONDOWN()
    ON_UPDATE_COMMAND_UI(ID_NAVIGATE_REPAINT, OnUpdateNavigateRepaint)
    ON_WM_CREATE()
    ON_COMMAND(ID_NAVIGATE_REPAINT, OnNavigateRepaint)
    ON_COMMAND(ID_POPUP_LOADLINK, OnPopupLoadLink)
    ON_COMMAND(ID_POPUP_ADDLINK2BOOKMARKS, OnPopupAddLinkToBookmarks)
    ON_COMMAND(ID_POPUP_LOADLINKNEWWINDOW, OnPopupLoadLinkNewWindow)
	ON_COMMAND(ID_POPUP_LOADFRAMENEWWINDOW, OnPopupLoadFrameNewWindow)
    ON_COMMAND(ID_POPUP_SAVELINKCONTENTS, OnPopupSaveLinkContentsAs)
    ON_COMMAND(ID_POPUP_COPYLINKCLIPBOARD, OnPopupCopyLinkToClipboard)
    ON_COMMAND(ID_POPUP_LOADIMAGE, OnPopupLoadImage)
    ON_COMMAND(ID_POPUP_SAVEIMAGEAS, OnPopupSaveImageAs)
    ON_COMMAND(ID_POPUP_COPYIMGLOC2CLIPBOARD, OnPopupCopyImageLocationToClipboard)
    ON_COMMAND(ID_POPUP_LOADIMAGEINLINE, OnPopupLoadImageInline)
    ON_COMMAND(ID_POPUP_ACTIVATEEMBED, OnPopupActivateEmbed)
    ON_COMMAND(ID_POPUP_SAVEEMBEDAS, OnPopupSaveEmbedAs)
    ON_COMMAND(ID_POPUP_COPYEMBEDLOC, OnPopupCopyEmbedLocationToClipboard)
	ON_COMMAND(ID_POPUP_SETASWALLPAPER, OnPopupSetAsWallpaper)
    ON_COMMAND(ID_POPUP_COPYEMBEDDATA, OnPopupCopyEmbedToClipboard)
    ON_COMMAND(ID_CANCEL_EDIT, OnDeactivateEmbed)
	ON_COMMAND(ID_POPUP_INTERNETSHORTCUT, OnPopupInternetShortcut)
	ON_COMMAND(ID_POPUP_MAILTO, OnPopupMailTo)
    //}}AFX_MSG_MAP
    // Standard printing commands
    ON_WM_SIZE()
    ON_COMMAND(ID_FILE_PRINT_PREVIEW, OnFilePrintPreview)
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
    ON_COMMAND(ID_DRAG_THIS_URL, OnCopyCurrentURL)
#ifdef EDITOR
	ON_UPDATE_COMMAND_UI(ID_DRAG_THIS_URL, OnCanInteract)
    ON_COMMAND(ID_POPUP_EDIT_IMAGE, OnPopupEditImage )       //Implemented in popup.cpp
    ON_COMMAND(ID_POPUP_EDIT_LINK, OnPopupLoadLinkInEditor ) //  "
    // Shift-F10 is same as right mouse button down
    ON_COMMAND( ID_LOCAL_POPUP, OnLocalPopup )
#endif //EDITOR
END_MESSAGE_MAP()

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

CNetscapeView::CNetscapeView()  
{
   m_pChild = NULL;
    //  Initialize some members.
    m_pDropTarget = NULL;
	m_pPreviewContext = NULL;

	//	We have no context yet.
	m_pContext = NULL;

    m_pSaveFileDlg = NULL;
#ifdef EDITOR
    // This is set to TRUE by derived class CEditView
    //  if we are actually an editor
    m_bIsEditor = FALSE;

    // Register our HTML-preserving clipboard format
    m_cfEditorFormat = RegisterClipboardFormat(NETSCAPE_EDIT_FORMAT);

    // Be sure BookmarkName clipboard format was registered also
    m_cfBookmarkFormat = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

    // Used to drag image reference from browser window to insert into editor
    m_cfImageFormat = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_IMAGE_FORMAT);

    // Construct our list of supported clipboard formats
    // Be sure size is same as number of formats!
    // *** This should be done at some centralized app-data!
    
    m_pClipboardFormats = new UINT[MAX_CLIPBOARD_FORMATS];
    ASSERT(m_pClipboardFormats);
    
    for( int i = 0; i < MAX_CLIPBOARD_FORMATS; i++ ){
        m_pClipboardFormats[i] = 0;
    }

    // This is stupid! Clipboard format array must be UINT,
    //  but CLIPFORMAT type is an unsigned short!
    // Editor's own format for HTML text
    m_pClipboardFormats[0] = (UINT)m_cfEditorFormat;
    // Bookmark format
    m_pClipboardFormats[1] = (UINT)m_cfBookmarkFormat;

    // Used to drag 
    m_pClipboardFormats[2] = (UINT)m_cfImageFormat;

    // These are from shlobj.h Not all used right now
#ifdef XP_WIN32
    m_pClipboardFormats[3] = (UINT)RegisterClipboardFormat(CFSTR_SHELLIDLIST);
    m_pClipboardFormats[4] = (UINT)RegisterClipboardFormat(CFSTR_NETRESOURCES);
    m_pClipboardFormats[5] = (UINT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
    m_pClipboardFormats[6] = (UINT)RegisterClipboardFormat(CFSTR_FILECONTENTS);
    m_pClipboardFormats[7] = (UINT)RegisterClipboardFormat(CFSTR_FILENAME);
    m_pClipboardFormats[8] = (UINT)RegisterClipboardFormat(CFSTR_PRINTERGROUP);
    m_pClipboardFormats[9] = (UINT)CF_UNICODETEXT;
#endif
    
    // Built-in Windows formats:
    m_pClipboardFormats[10] = (UINT)CF_TEXT;
#ifdef _IMAGE_CONVERT
    m_pClipboardFormats[11] = (UINT)CF_DIB;
    ASSERT( MAX_CLIPBOARD_FORMATS == 12 );
#else
    ASSERT( MAX_CLIPBOARD_FORMATS == 11 );
#endif //_IMAGE_CONVERT


            // m_pClipboardFormats[10] = CF_BITMAP;
            // m_pClipboardFormats[11] = CF_DIB;
            // m_pClipboardFormats[12] = CF_METAFILEPICT;

    // ARE YOU ADDING A CLIPBOARD FORMAT???
    //  don't forget to update MAX_CLIPBOARD_FORMATS in NETSVW.H

#endif // EDITOR
}

// No longer accept drop events as we are going away
//
CNetscapeView::~CNetscapeView()  
{
    if(m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }

#ifdef EDITOR
    if ( m_pClipboardFormats )
        delete [] m_pClipboardFormats;
#endif
}

BOOL CNetscapeView::PreCreateWindow(CREATESTRUCT & cs) 
{

    cs.lpszClass = AfxRegisterWndClass(
        CS_BYTEALIGNWINDOW |
        CS_BYTEALIGNCLIENT |

        // Although a private DC is convenient to use, it is expensive
        //  in terms of system resources, requiring 800 or more bytes
        //  to store. Private DCs are recommended when performance
        //  considerations outweigh storage costs. 
        //      - MSDN
        // 800 / (64 * 1024) = 1.2% of GDI per window.
        // Win16 can do without?  We'll see.
        CS_OWNDC |

        CS_DBLCLKS);
    
	//	Do not add scroll bars anymore.
    cs.style = cs.style | WS_HSCROLL | WS_VSCROLL | WS_CLIPSIBLINGS| WS_CHILD;

    return (CGenericView::PreCreateWindow(cs));
}

/////////////////////////////////////////////////////////////////////////////
// CNetscapeView drawing

void CNetscapeView::OnPrepareDC(CDC *pDC, CPrintInfo * pInfo /* = NULL */) 
{
  //  Color is now set in OnEraseBkgnd  

#ifdef ONLY_GRAY_BACKGROUND
    if (!pInfo) 
        pDC->SetBkColor(RGB(192, 192, 192));
#else
    pDC->SetBkMode(TRANSPARENT);
#endif
          
}

/////////////////////////////////////////////////////////////////////////////
// CNetscapeView printing
void CNetscapeView::OnFilePrintPreview()
{
	if(GetContext() == NULL ||
		GetFrame()->GetMainWinContext() == NULL ||
		GetFrame()->GetMainWinContext()->GetPane() == NULL ||
		GetFrame()->GetActiveWinContext() == NULL ||
		GetFrame()->GetActiveWinContext()->GetPane() == NULL)	{

		return;
	}
    // Note: We used to force saving an editor page here,
    //  but we don't need to with new tempfile mechanism

	//	If we're a grid cell, then we have to have the frame's main view
	//		handle this, or MFC gets confused...
	if(GetContext()->IsGridCell() == TRUE)	{
		//	After beta 2, however.
		((CNetscapeView *)GetFrame()->GetMainWinContext()->GetView())->OnFilePrintPreview();
		return;
	}

	//	However, if we're a grid parent, we can't allow this unless we are
	//		not the active context of the frame, and the active context is
	//		not a grid cell.
	if(GetContext()->IsGridParent() == TRUE)	{
		//	Must not be active.
		if(GetContext()->GetFrame()->GetActiveContext() == GetContext())	{
			return;
		}

		//	Active must not be Grid Parent.
		if(GetFrame()->GetActiveContext()->IsGridParent() == TRUE)	{
			return;
		}
	}

	//	Remember that we've started print preview.
	//	Do this first.
	m_bInPrintPreview = TRUE;

	//	Get rid of the URL bar for print preview.
	LPCHROME pChrome = GetFrame()->GetChrome();
	if(pChrome) {
	    pChrome->ViewCustToolbar(FALSE);
	}
	// this is not necessary now, but if we ever save things not
	// related to the customizable toolbar it will be
	GetFrame()->SaveBarState();

#ifdef MOZ_MAIL_NEWS
#ifdef EDITOR
    // Deal with Message Composer toolbars as well
    MWContext *pMWContext = GetContext()->GetContext();
    if( pMWContext && pMWContext->bIsComposeWindow )
    {
        // Get the Address widget and hide it if its visible
        // Note: For minimum change here, we will always restore it when done previewing
        CComposeBar *pAddressBar = ((CComposeFrame*)GetFrame())->GetComposeBar();
        if( pAddressBar && pAddressBar->IsVisible() )
        {
            pAddressBar->OnToggleShow();    
        }

        // Hide the Composer's formatting toolbar,
        //  and remember state so we restore only if currently visible
	    m_bRestoreComposerToolbar = FALSE;
        CEditToolBarController * pController = 
		    (CEditToolBarController *)GetParentFrame()->SendMessage(WM_TOOLCONTROLLER);
	    if( pController )
        {
            CComboToolBar * pToolBar = pController->GetCharacterBar();
            if( pToolBar )
            {
                m_bRestoreComposerToolbar = pToolBar->IsVisible();
                if( m_bRestoreComposerToolbar )
                    pController->ShowToolBar( FALSE, pToolBar );
            }
        }
    }
#endif
#endif //MOZ_MAIL_NEWS

	//	Must outlive this function.
	CPrintPreviewState *pState = new CPrintPreviewState;

	if(!DoPrintPreview(AFX_IDD_PREVIEW_TOOLBAR, this, RUNTIME_CLASS(CNetscapePreviewView), pState))	{
		AfxMessageBox(AFX_IDP_COMMAND_FAILURE);
		delete pState;
		if(pChrome) {
		    pChrome->ViewCustToolbar(TRUE);
		}
		GetFrame()->RestoreBarState();

		//	Do this last.
		m_bInPrintPreview = FALSE;
	}
}

void CNetscapeView::OnEndPrintPreview(CDC *pDC, CPrintInfo *pInfo, POINT pXY, CPreviewView *pPreView)	{
	CNetscapePreviewView *pView = (CNetscapePreviewView *)pPreView;

	if (pView->m_pPrintView != NULL)
		((CNetscapeView *)pView->m_pPrintView)->OnEndPrinting(pDC, pInfo);

	CFrameWnd* pParent = (CFrameWnd*)GetFrame()->GetFrameWnd(); //AfxGetThread()->m_pMainWnd;
	ASSERT_VALID(pParent);
	ASSERT(pParent->IsKindOf(RUNTIME_CLASS(CFrameWnd)));

	// restore the old main window
	pParent->OnSetPreviewMode(FALSE, pView->m_pPreviewState);

	// Force active view back to old one
	pParent->SetActiveView(pView->m_pPreviewState->pViewActiveOld);
	if (pParent != GetParentFrame())
		OnActivateView(TRUE, this, this);   // re-activate view in real frame
    
    //CLM: This fixes the print preview crash OnClose
    //     Some MFC weirdness, but we have to delete this
    //     and not let pPreView try to do it 
    delete pView->m_pPreviewState;
    pView->m_pPreviewState = NULL;

	pView->DestroyWindow();     // destroy preview view
			// C++ object will be deleted in PostNcDestroy

	// restore main frame layout and idle message
	pParent->RecalcLayout();
	pParent->SendMessage(WM_SETMESSAGESTRING, (WPARAM)AFX_IDS_IDLEMESSAGE, 0L);
	pParent->UpdateWindow();

	//	If we have a print context, get rid of it here.
	if(m_pPreviewContext != NULL)	{
		//	If it's still loading, just have it cancel.
		//	It will delete itself.
		if(m_pPreviewContext->GetDisplayMode() == BLOCK_DISPLAY)	{
			m_pPreviewContext->CancelPrintJob();
		}
		else	{
			//	We can get rid of it right now.
			m_pPreviewContext->DestroyContext();
		}
		m_pPreviewContext = NULL;
	}
	LPCHROME pChrome = GetFrame()->GetChrome();
	if(pChrome) {
	    pChrome->ViewCustToolbar(TRUE);
	}

	// this is not necessary now, but if we ever save things not
	// related to the customizable toolbar it will be
	GetFrame()->RestoreBarState();

#ifdef MOZ_MAIL_NEWS
#ifdef EDITOR
    // Restore Message Composer toolbars as well
    MWContext *pMWContext = GetContext()->GetContext();
    if( pMWContext && pMWContext->bIsComposeWindow )
    {
        // Note: We always restore Address widget when done previewing
        CComposeBar *pAddressBar = ((CComposeFrame*)GetFrame())->GetComposeBar();
        if( pAddressBar && !pAddressBar->IsVisible() )
        {
            pAddressBar->OnToggleShow();    
        }

        // Restore Composer format/character toolbar only if it was visible before
        if( m_bRestoreComposerToolbar )
        {
            CEditToolBarController * pController = 
		        (CEditToolBarController *)GetParentFrame()->SendMessage(WM_TOOLCONTROLLER);
	        if( pController )
            {
                CComboToolBar * pToolBar = pController->GetCharacterBar();
                if( pToolBar )
                    pController->ShowToolBar( TRUE, pToolBar );
            }
        }
        m_bRestoreComposerToolbar = FALSE;
    }
#endif // EDITOR
#endif // MOZ_MAIL_NEWS

	//	Do this last.
	m_bInPrintPreview = FALSE;
}

BOOL CNetscapeView::OnPreparePrinting(CPrintInfo *pInfo)	{
	//	This only gets called in print preview mode.
	//	Never in print only mode.
	return(DoPreparePrinting(pInfo));
}

void CNetscapeView::OnPrint(CDC *pDC, CPrintInfo *pInfo)	{
	//	This only gets called in print preview mode.
	//	Never in print only mode.
	if(m_pPreviewContext == NULL)	{
        
        CAbstractCX *pContext = GetFrame()->GetActiveContext();

		//	Since we have no preview context, this means we're
		//		starting to preview a page.
		//	We need to load it.
		//	We must call on behalf of the active context, or MFC get's confused.
		if( !pContext || pContext->CanCreateUrlFromHist() == FALSE)	{
			return;
		}
        // This also sets m_pPreviewContext once called to non NULL.
        //  Always pass the WYSIWYG attribute for printing URLs (javascript).
        SHIST_SavedData SavedData;
        URL_Struct *pUrl = pContext->CreateUrlFromHist(TRUE, &SavedData, TRUE);
		pUrl->position_tag = 0;

        char *pDisplayUrl = NULL;
#ifdef EDITOR
        // Save actual address we want to show in header or footer
        //  to pass on to print context
        if( pUrl->address )
            pDisplayUrl = XP_STRDUP(pUrl->address);

        // If necessary (Mail Compose Window, new page, or changes in current page),
        //  copy current page to a temp file and switch
        //  to that URL address in URL_Struct
        if( !FE_PrepareTempFileUrl(pContext->GetContext(), pUrl) )
        {
            XP_FREEIF(pDisplayUrl);
            // Failed to save to the temp file - abort
            return;
        }
#endif
		// Copy the necessary information into the URL's saved data so that we don't
		// make a copy of the plug-ins when printing
		NPL_PreparePrint(pContext->GetContext(), &pUrl->savedData);

		CPrintCX::PreviewAnchorObject(m_pPreviewContext, pUrl, this, pDC, pInfo, &SavedData, pDisplayUrl);
        XP_FREEIF(pDisplayUrl);
	}

	//	Either the page is loading in the print context, or it's done loading.
	//	In any event, attempt to have it print the page.
	m_pPreviewContext->PrintPage(pInfo->m_nCurPage, pDC->GetSafeHdc(), pInfo);
}

BOOL CNetscapeView::DoPrintPreview(UINT nIDResource, CView* pPrintView,
	CRuntimeClass* pPreviewViewClass, CPrintPreviewState* pState)
{
	ASSERT_VALID_IDR(nIDResource);
	ASSERT_VALID(pPrintView);
	ASSERT(pPreviewViewClass != NULL);            
#ifdef XP_WIN32	
	ASSERT(pPreviewViewClass->IsDerivedFrom(RUNTIME_CLASS(CPreviewView)));
#endif
	ASSERT(pState != NULL);

	CFrameWnd* pParent = (CFrameWnd*)GetFrame()->GetFrameWnd(); // AfxGetThread()->m_pMainWnd;
	ASSERT_VALID(pParent);
	ASSERT(pParent->IsKindOf(RUNTIME_CLASS(CFrameWnd)));

	CCreateContext context;
	context.m_pCurrentFrame = pParent;
	context.m_pCurrentDoc = GetDocument();
	context.m_pLastView = this;

	// Create the preview view object
	CNetscapePreviewView* pView = (CNetscapePreviewView*)pPreviewViewClass->CreateObject();
	if (pView == NULL)
	{
		TRACE0("Error: Failed to create preview view.\n");
		return FALSE;
	}
	ASSERT(pView->IsKindOf(RUNTIME_CLASS(CPreviewView)));
	pView->m_pPreviewState = pState;        // save pointer

	pParent->OnSetPreviewMode(TRUE, pState);    // Take over Frame Window

	// Create the toolbar from the dialog resource
	pView->m_pToolBar = new CDialogBar;
	if (!pView->m_pToolBar->Create(pParent, MAKEINTRESOURCE(nIDResource),
		CBRS_TOP, AFX_IDW_PREVIEW_BAR))
	{
		TRACE0("Error: Preview could not create toolbar dialog.\n");
		pParent->OnSetPreviewMode(FALSE, pState);   // restore Frame Window
		delete pView->m_pToolBar;       // not autodestruct yet
		pView->m_pToolBar = NULL;
		pView->m_pPreviewState = NULL;  // do not delete state structure
        // (because pState is deleted by caller)
		delete pView;
		return FALSE;
	}
	pView->m_pToolBar->m_bAutoDelete = TRUE;    // automatic cleanup

	// Create the preview view as a child of the App Main Window.  This
	// is a sibling of this view if this is an SDI app.  This is NOT a sibling
	// if this is an MDI app.

	if (!pView->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
		CRect(0,0,0,0), pParent, AFX_IDW_PANE_FIRST, &context))
	{
		TRACE0("Error: couldn't create preview view for frame.\n");
		pParent->OnSetPreviewMode(FALSE, pState);   // restore Frame Window
        // Note:
        //CLM: Delete here -- looks better!
        delete pView->m_pPreviewState;
		pView->m_pPreviewState = NULL;  // do not delete state structure
        // (because pState is deleted by caller)
		delete pView;
		return FALSE;
	}

	// Preview window shown now

	pState->pViewActiveOld = pParent->GetActiveView();
	CView* pActiveView = pParent->GetActiveFrame()->GetActiveView();
	if (pActiveView != NULL)
		((CNetscapeView *)pActiveView)->OnActivateView(FALSE, pActiveView, pActiveView);

	if (!pView->SetPrintView(pPrintView))
	{
		pView->OnPreviewClose();
		return TRUE;            // signal that OnEndPrintPreview was called
	}

	pParent->SetActiveView(pView);  // set active view - even for MDI

	// update toolbar and redraw everything
	pView->m_pToolBar->SendMessage(WM_IDLEUPDATECMDUI, (WPARAM)TRUE);
	pParent->RecalcLayout();            // position and size everything
	pParent->UpdateWindow();

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CNetscapeView diagnostics

#ifdef _DEBUG
void CNetscapeView::AssertValid() const
{
    CGenericView::AssertValid();
}

void CNetscapeView::Dump(CDumpContext& dc) const
{
    CGenericView::Dump(dc);
}
#endif //_DEBUG

CGenericDoc* CNetscapeView::GetDocument()	{
	CGenericDoc *pRetval = NULL;
	if(m_pContext != NULL)	{
    	pRetval = m_pContext->GetDocument();
	}

	return(pRetval);
}


/////////////////////////////////////////////////////////////////////////////
// CNetscapeView message handlers

int CNetscapeView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CGenericView::OnCreate(lpCreateStruct) == -1)
        return -1;

    // initialize the drop target
    if(!m_pDropTarget) {
        m_pDropTarget = new CViewDropTarget;
        m_pDropTarget->Register(this);
    }
    //Ok, I'm confused!
    // CMainFrame does DragAcceptFiles(TRUE),
    //  which should enable drop for all child windows
    // But doing DragAcceptFiles(TRUE) actually 
    //  prevents files dropped into the view from being accepted!
    //  And putting DragAcceptFiles(FALSE) here enables it!
    //  Leaving it out also enables it, so lets do that
    //
    // DragAcceptFiles(FALSE);
    return 0;
}

// Force a window repaint
// Don't do this by hand (i.e. by calling LO_RefreshArea()) or we will be
//  unable to cache the pDC
//
void CNetscapeView::OnNavigateRepaint()
{
    InvalidateRect(NULL, FALSE);
    UpdateWindow();
}

void CNetscapeView::OnUpdateNavigateRepaint(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

// Block of Editor functions removed from here
// Following are formerly editor functions that are useful in Browser

#ifdef EDITOR
// Use this for any commands that are enabled most of the time
void CNetscapeView::OnCanInteract(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CAN_INTERACT);
}

void CNetscapeView::OnLocalPopup()
{
    if( CAN_INTERACT ) {
        CPoint CaretPoint = GetCaretPos();
        // Send message to appropriate view's OnRButtonDown to bring up
        //   context menu at caret location
        PostMessage(WM_RBUTTONDOWN, 0, MAKELPARAM(CaretPoint.x, CaretPoint.y));
    }
}

void CNetscapeView::SpawnExternalEditor(MWContext *pMWContext, const char *pPrefName, char *pURL, UINT nIDCaption)
{
    if( !pMWContext || !pPrefName ){
        return;
    }

    // Get Editor application name from preferences
    char * pEditor = NULL;
    PREF_CopyCharPref(pPrefName, &pEditor);

    if( pEditor == NULL || pEditor[0] == '\0' ){
GET_EDITOR:
        // If no Editor name in preferences, get from OpenFile dialog
        pEditor = wfe_GetExistingFileName(this->m_hWnd, szLoadString(nIDCaption), EXE, TRUE);
        if( pEditor ){
            // Save new editor name
            PREF_SetCharPref(pPrefName, pEditor);
        }
    }
    if(pEditor == NULL) return;

    char *pLocalName = NULL;
    CString csMsg;
    BOOL bError = TRUE;

    CString csURL(pURL);
    CString csEditor(pEditor);

    if( XP_ConvertUrlToLocalFile(csURL, &pLocalName) ){
        CString csCommand;
        // Enclose any string containing spaces with quotes for long filenames
        if ( csURL.Find(' ') > 0 ){
            csCommand = csEditor + " \"" + pLocalName + "\"";    
        } else {
            csCommand = csEditor + " " + pLocalName;
        }
		UINT nSpawn = WinExec(csCommand, SW_SHOW);
		if(nSpawn < 32)	{
            switch(nSpawn) {
                case 2:                                      
                case 3:
                    // Instead of useless error message,
                    //  jump to prompt user to enter external Editor application
                    goto GET_EDITOR;
                default:
                    char szMsg[80];
                    sprintf(szMsg, szLoadString(IDS_UNABLE_TO_LAUNCH_EDITOR), nSpawn);
                    csMsg = szMsg;
                    break;
            }        
        } else {
            bError = FALSE;
        }
    } else {
        AfxFormatString1(csMsg, IDS_ERR_SRC_NOT_FOUND, pLocalName);  
    }
    if( pLocalName ){
        XP_FREE(pLocalName);
    }
    if( bError ){
        MessageBox(csMsg, szLoadString(IDS_SPAWNING_APP),
                   MB_OK | MB_ICONEXCLAMATION);
    }

    XP_FREE(pEditor);
}

void CNetscapeView::OnEditSource()
{
    MWContext *pMWContext = GET_MWCONTEXT;
    if( !FE_SaveDocument(pMWContext) ){
        return;
    }

    // Base URL is the address of current document
    History_entry * hist_ent = SHIST_GetCurrent(&(pMWContext->hist));
    if ( hist_ent == NULL || hist_ent->address == NULL ){
        return;
    }
    // Launch an external editor for current page
    SpawnExternalEditor(pMWContext, "editor.html_editor", hist_ent->address, IDS_SELECT_HTML_EDITOR);
}

void CNetscapeView::EditImage(char *pImage)
{
    MWContext *pMWContext = GET_MWCONTEXT;
    if( pImage == NULL || XP_STRLEN(pImage) == 0 ||
        pMWContext == NULL)
    {
        return;
    }

    // Base URL is the address of current document
    History_entry * hist_ent = SHIST_GetCurrent(&(pMWContext->hist));
    if ( hist_ent == NULL || hist_ent->address == NULL )
    {
        return;
    }

    char * pURL = NET_MakeAbsoluteURL(hist_ent->address, pImage);

    // Check if image is remote reference - TODO: Automatically download image to edit it
    if( pURL )
    {
        if( !NET_IsLocalFileURL(pURL) )
        {
            CWnd *pWnd = GetContext()->GetDialogOwner();
            pWnd->MessageBox(szLoadString(IDS_IMAGE_IS_REMOTE), 
                       szLoadString(IDS_EDIT_IMAGE_CAPTION),
                       MB_OK | MB_ICONEXCLAMATION);
            XP_FREE(pURL);
            return;
        }
        SpawnExternalEditor(pMWContext, "editor.image_editor", pURL, IDS_SELECT_IMAGE_EDITOR);
        XP_FREE(pURL);
    }
}

// *** end of "formerly editor functions"
#endif /* EDITOR */

void CNetscapeView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    // Pass it off to the context to handle if it can.
    if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)
        GetContext()->OnKeyUp(nChar, nRepCnt, nFlags);
}

void CNetscapeView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{         
    
    // Pass it off to the context to handle if it can.
    if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE) {
        if(GetContext()->OnKeyDown(nChar, nRepCnt, nFlags))
            return;
    }
     
    switch(nChar) {
    case ' ':
    case VK_NEXT:
        // page down
        OnVScroll(SB_PAGEDOWN, 0, NULL);
        break;
    case VK_BACK:
    case VK_PRIOR:
        // page up
        OnVScroll(SB_PAGEUP, 0, NULL);
        break;
    case VK_UP:
        // line up
        OnVScroll(SB_LINEUP, 0, NULL);
        break;
    case VK_DOWN:
        // line down
        OnVScroll(SB_LINEDOWN, 0, NULL);
        break;
    case VK_RIGHT:
        // line right
        OnHScroll(SB_LINERIGHT, 0, NULL);
        break;
    case VK_LEFT:
        // line left
        OnHScroll(SB_LINELEFT, 0, NULL);
        break;
    case VK_ESCAPE: {
            CWinCX *pWinCX = GetContext();
            //  escape, kill off any selected items.
            if(pWinCX && pWinCX->m_pSelected != NULL) {
			    OnDeactivateEmbed();
            }
        }
        break;
	case VK_HOME:
		if (::GetKeyState(VK_CONTROL) < 0)
			OnVScroll(SB_TOP, 0, NULL);
		else
			OnHScroll(SB_TOP, 0, NULL);
		break;
	case VK_END:
		if (::GetKeyState(VK_CONTROL) < 0)
			OnVScroll(SB_BOTTOM, 0, NULL);
		else
			OnHScroll(SB_BOTTOM, 0, NULL);
		break;
    default:
        CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
    }
}

void CNetscapeView::OnInitialUpdate() 
{
	//  Assign in our document, since no one else has bothered
	if(m_pDocument == NULL)	{
		m_pContext->GetDocument()->AddView(this);
	}
  
  CGenericView::OnInitialUpdate();  
}


void CNetscapeView::OnDeactivateEmbed() {
	if(m_pContext != NULL)	{
		m_pContext->OnDeactivateEmbedCX();
	}
}

//
// Focus management to support international forms
//
void CNetscapeView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) 
{
	//	Remember focus before we activate.
	HWND hWndFocus = m_hWndFocus;
    CGenericView::OnActivateView(bActivate, pActivateView, pDeactiveView);

    if(bActivate) {
        //	Restore focus to child if exists.
        if(hWndFocus && ::IsWindow(hWndFocus))	{
            ::SetFocus(hWndFocus);
		}
    } else {
        HWND tmp = ::GetFocus();

        if(tmp && ::IsChild(m_hWnd, tmp))
            m_hWndFocus = tmp;
        else
            m_hWndFocus = NULL;

    }
    //TRACE2("CNetscapeView::OnActivateView: bActivate = %d, m_hWndFocus = %d\n", bActivate, m_hWndFocus);
}

void CNetscapeView::OnActivateFrame(UINT nState, CFrameWnd* /*pFrameWnd*/)
{
    if (nState == WA_INACTIVE) {
        HWND tmp = ::GetFocus();

        if(tmp && ::IsChild(m_hWnd, tmp))
            m_hWndFocus = tmp;
        else
            m_hWndFocus = NULL;
    }
    //TRACE1("CNetscapeView::OnActivateFrame: m_hWndFocus = %d\n", m_hWndFocus);
}

FILE * fpSeriousHackery = NULL;


// We just got focus
// See who wants it
//
void CNetscapeView::OnSetFocus(CWnd *pOldWin)   
{
   if (m_pChild)
   {
      m_pChild->SetFocus();
      return;
   }
    // See if an active OLE item used to have focus
	if(GetDocument() != NULL)	{
	    CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)GetDocument()->GetInPlaceActiveItem(this);
    	if(pItem != NULL && pItem->GetItemState() == COleClientItem::activeUIState)   {
	        //  Active item, give it focus.
	        CWnd *pWnd = pItem->GetInPlaceWindow();
	        if(pWnd != NULL)    {
	            pWnd->SetFocus();
	            return;
	        }
	    }
	}

    // Did a form element used to have focus?
    if(m_hWndFocus && ::IsWindow(m_hWndFocus)) {
        ::SetFocus(m_hWndFocus);
        return;
    }                           

    // No one wants it.  Just pass it to the base class
    CGenericView::OnSetFocus(pOldWin);
}

//////////////////////////////////////////////////////////////////////////////
// Drag from the Bitmap Menu item
void CNetscapeView::OnCopyCurrentURL()
{
    if ( GetContext() ) {
        GetContext()->CopyCurrentURL();
    }
}

/////////////////////////////////////////////////////////////////////////
CViewDropSource::CViewDropSource(UINT nDragType)
    : m_nDragType(nDragType) 
{
}

SCODE CViewDropSource::GiveFeedback(DROPEFFECT dropEffect)
{
    if( dropEffect != DROPEFFECT_COPY &&
        dropEffect != DROPEFFECT_MOVE ){
        	return COleDropSource::GiveFeedback(dropEffect);
    }
    BOOL bCopy = (dropEffect == DROPEFFECT_COPY);

    switch ( m_nDragType ) {
        case FE_DRAG_TEXT:
            SetCursor(theApp.LoadCursor( bCopy ?
                        IDC_TEXT_COPY : IDC_TEXT_MOVE));
            break;
        case FE_DRAG_HTML:
            SetCursor(theApp.LoadCursor( bCopy ?
                        IDC_HTML_COPY : IDC_HTML_MOVE));
            break;
        case FE_DRAG_TABLE:
            SetCursor(theApp.LoadCursor( bCopy ?
                        IDC_TABLE_COPY : IDC_TABLE_MOVE));
            break;
        case FE_DRAG_LINK:
            SetCursor(theApp.LoadCursor( bCopy ?
                        IDC_LINK_COPY : IDC_LINK_MOVE));
            break;
        case FE_DRAG_IMAGE:
            SetCursor(theApp.LoadCursor( bCopy ?
                        IDC_IMAGE_COPY : IDC_IMAGE_MOVE));
            break;
        default:
        	return COleDropSource::GiveFeedback(dropEffect);
    }
    return NOERROR;
}

void CNetscapeView::OnSize ( UINT nType, int cx, int cy )
{
    CGenericView::OnSize ( nType, cx, cy );
    if ( m_pChild )
    {
        ShowScrollBar ( SB_BOTH, FALSE );
        CRect rect ( 0, 0, cx, cy );
        GetClientRect(rect);
        m_pChild->MoveWindow ( rect );
    }
}

//////////////////////////////////////////////////////////////////////////////
CViewDropTarget::CViewDropTarget() 
{
}


typedef struct drag_closure {
    char		* tmpUrl;
    char		** jsUrl;
    CNetscapeView	* cView;
} drag_closure;

/* Mocha has processed the event handler for the drop event.  Now come back and execute
 * the drop files call if EVENT_OK or cleanup and go away.
 */
static void
win_on_drop_callback(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{
    char * tmpUrl;
    
    drag_closure * pClose = (drag_closure *) pObj;
    tmpUrl = pClose->tmpUrl;

    // make sure document hasn't gone away or event cancelled
    if(status == EVENT_PANIC || status == EVENT_CANCEL) {
	XP_FREE(pClose->jsUrl[0]);
        XP_FREE(pClose->jsUrl);
        XP_FREE(pClose);
        XP_FREE(tmpUrl);
	return;
    }

    // find out who we are
    CNetscapeView * cView = pClose->cView;
    
    cView->GetContext()->NormalGetUrl(tmpUrl);
   
    XP_FREE(pClose->jsUrl[0]);
    XP_FREE(pClose->jsUrl);
    XP_FREE(pClose);
    XP_FREE(tmpUrl);
    return;
}


BOOL CViewDropTarget::OnDrop(CWnd* pWnd, 
                COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{

    if(!pDataObject || !pWnd)
        return(FALSE);

#ifdef XP_WIN32
    // we only handle text at the moment
    if(! (pDataObject->IsDataAvailable(CF_TEXT) || pDataObject->IsDataAvailable(CF_UNICODETEXT)))
        return(FALSE);
#else
    // we only handle text at the moment
    if(!pDataObject->IsDataAvailable(CF_TEXT))
        return(FALSE);
#endif

    CNetscapeView * cView = (CNetscapeView *) pWnd;

	BOOL bUseUnicodeData = FALSE;
	char * tmpUrl;

#ifdef XP_WIN32
	// in the case of WIN32, Try CF_UNICODETEXT first
	int16 wincsid = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo( cView->GetContext()->GetContext() ));
	if( (CS_USER_DEFINED_ENCODING != (wincsid & ~CS_AUTO)) && 
		pDataObject->IsDataAvailable(CF_UNICODETEXT)
	  )
	{	// we have Unicode data and we want to use it.
		HGLOBAL hString = pDataObject->GetGlobalData(CF_UNICODETEXT);
		if(hString)
		{
			// get a pointer to the actual bytes
			char * pString = (char *) GlobalLock(hString);
			if(pString)
			{
				// Now, let's convert the Unicode text into the datacsid encoding
				int ucs2len = CASTINT(INTL_UnicodeLen((INTL_Unicode*)pString));
				int	mbbufneeded = CASTINT(INTL_UnicodeToStrLen(wincsid, 
															(INTL_Unicode*)pString, 
															ucs2len));
				if(NULL != (tmpUrl = (char*)XP_ALLOC(mbbufneeded + 1)))
				{
					INTL_UnicodeToStr(wincsid, (INTL_Unicode*)pString, ucs2len, 
											(unsigned char*) tmpUrl, mbbufneeded + 1);
					bUseUnicodeData = TRUE;
				}
				GlobalUnlock(hString);
			}
		}
	}
#endif
	if(! bUseUnicodeData)
	{
		// get the data
		HGLOBAL hString = pDataObject->GetGlobalData(CF_TEXT);
		if(hString == NULL)
			return(FALSE);

		// get a pointer to the actual bytes
		char *  pString = (char *) GlobalLock(hString);    
		if(!pString)
			return(FALSE);

		tmpUrl = XP_STRDUP(pString);

		GlobalUnlock(hString);
	}

    drag_closure * pClosure = XP_NEW_ZAP(drag_closure);
    pClosure->cView = cView;
    pClosure->tmpUrl = tmpUrl;
    pClosure->jsUrl = (char**)XP_ALLOC(sizeof(char*));
    pClosure->jsUrl[0] = XP_STRDUP(tmpUrl);

    XY Point;

    CWinCX *pContext = cView->GetContext();
    if (pContext) {
	pContext->ResolvePoint(Point, point);
    }
    else {
	Point.x = 0;
	Point.y = 0;
    }

    cView->ClientToScreen( &point );

    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_DRAGDROP;
    event->which = 0;
    event->x = 0;
    event->y = 0;
    event->docx = Point.x;
    event->docy = Point.y;
    event->screenx = point.x;
    event->screeny = point.y;
    event->modifiers = (GetKeyState(VK_SHIFT) < 0 ? EVENT_SHIFT_MASK : 0) 
		    | (GetKeyState(VK_CONTROL) < 0 ? EVENT_CONTROL_MASK : 0) 
		    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
    event->dataSize = 1;
    event->data = (void *)pClosure->jsUrl;

    ET_SendEvent(cView->GetContext()->GetContext(), 0, event, win_on_drop_callback, pClosure);
    return(TRUE);

    //Mocha will handle cleanup and free tmpUrl when it calls back in.
}


DROPEFFECT CViewDropTarget::OnDragEnter(CWnd* pWnd, 
                COleDataObject* pDataObject, DWORD dwKeyState, CPoint point )
{
    m_nIsOKDrop = FALSE;

#ifdef XP_WIN32
    // we only handle text at the moment
    if(! (pDataObject->IsDataAvailable(CF_TEXT) || pDataObject->IsDataAvailable(CF_UNICODETEXT)))
        return(FALSE);
#else
    // we only handle text at the moment
    if(!pDataObject->IsDataAvailable(CF_TEXT))
        return(FALSE);
#endif

    // can't drag onto ourselves
    CNetscapeView * cView = (CNetscapeView *) pWnd;
    if(cView && cView->GetContext() && cView->GetContext()->IsDragging())
        return(FALSE);

    // looks like its OK
    m_nIsOKDrop = TRUE;

    // Default is to copy
    return(DROPEFFECT_COPY);

}

DROPEFFECT CViewDropTarget::OnDragOver(CWnd* pWnd, 
                COleDataObject* pDataObject, DWORD dwKeyState, CPoint point )
{
    if(!m_nIsOKDrop)
        return(DROPEFFECT_NONE);

    return(DROPEFFECT_COPY);

}

void CViewDropTarget::OnDragLeave(CWnd* pWnd)
{
}

