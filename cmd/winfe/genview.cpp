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

// genview.cpp : implementation file
//

#include "stdafx.h"
#include "np.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifdef EDITOR
#include "mainfrm.h"    // Need this to access CMainFrame::OnLoadHomePage()
//#include "edres1.h"
#endif
#include "edt.h"
#include "button.h"
#include "libevent.h"
#include "findrepl.h"

/////////////////////////////////////////////////////////////////////////////
// CGenericView

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CGenericView, CView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

CGenericView::CGenericView()
{
    // no one has focus yet
    m_hWndFocus = NULL;
    m_pContext = NULL;
    m_bInPrintPreview = FALSE;
    m_bRestoreComposerToolbar = FALSE;
    m_hCtlBrush = NULL;
}

CGenericView::~CGenericView()
{
	//      when the view goes down, it would be a good idea to let the context
	//              know.
	if(GetContext() != NULL)        {
		GetContext()->ClearView();
	}

    if(m_hCtlBrush)
	DeleteObject(m_hCtlBrush);

}


BEGIN_MESSAGE_MAP(CGenericView, CView)
	//{{AFX_MSG_MAP(CGenericView)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
    ON_WM_MOUSEACTIVATE()
    ON_WM_CTLCOLOR()
	ON_WM_MOVE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_NCPAINT()
    ON_WM_NCCALCSIZE()

	ON_COMMAND(ID_FILE_MAILTO, OnFileMailto)
	ON_UPDATE_COMMAND_UI(ID_FILE_MAILTO, OnUpdateFileMailto)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPEN, OnUpdateFileOpen)
	ON_COMMAND(ID_NETSCAPE_SAVE_AS, OnNetscapeSaveAs)
	ON_UPDATE_COMMAND_UI(ID_NETSCAPE_SAVE_AS, OnUpdateNetscapeSaveAs)
	ON_COMMAND(ID_FILE_SAVEFRAME_AS, OnNetscapeSaveFrameAs)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVEFRAME_AS, OnUpdateNetscapeSaveFrameAs)
	ON_COMMAND(ID_NAVIGATE_BACK, OnNavigateBack)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_BACK, OnUpdateNavigateBack)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_FORWARD, OnUpdateNavigateForward)
	ON_COMMAND(ID_NAVIGATE_FORWARD, OnNavigateForward)
	ON_COMMAND(ID_NAVIGATE_RELOAD, OnNavigateReload)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_RELOAD, OnUpdateNavigateReload)
	ON_COMMAND(ID_VIEW_LOADIMAGES, OnViewLoadimages)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOADIMAGES, OnUpdateViewLoadimages)
	ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT, OnUpdateFilePrint)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT_PREVIEW, OnUpdateFilePrintPreview)
	ON_COMMAND(ID_EDIT_FINDINCURRENT, OnEditFindincurrent)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FINDINCURRENT, OnUpdateEditFindincurrent)
	ON_COMMAND(ID_EDIT_WITHFRAME_FINDINCURRENT, OnEditFindincurrent)
	ON_UPDATE_COMMAND_UI(ID_EDIT_WITHFRAME_FINDINCURRENT, OnUpdateEditWithFrameFindincurrent)
	ON_COMMAND(ID_EDIT_FINDAGAIN, OnEditFindAgain)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FINDAGAIN, OnUpdateEditFindAgain)
	ON_COMMAND(ID_NAVIGATE_INTERRUPT, OnNavigateInterrupt)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_INTERRUPT, OnUpdateNavigateInterrupt)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnSelectAll)
	ON_COMMAND(ID_FILE_VIEWSOURCE, OnFileViewsource)
	ON_UPDATE_COMMAND_UI(ID_FILE_VIEWSOURCE, OnUpdateFileViewsource)
	ON_COMMAND(ID_FILE_DOCINFO, OnFileDocinfo)
	ON_UPDATE_COMMAND_UI(ID_FILE_DOCINFO, OnUpdateFileDocinfo)
	ON_COMMAND(ID_VIEW_PAGESERVICES, OnViewPageServices)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PAGESERVICES, OnUpdateViewPageServices)
	ON_COMMAND(ID_GO_HOME, OnGoHome)
	ON_UPDATE_COMMAND_UI(ID_GO_HOME, OnUpdateGoHome)
	ON_COMMAND(ID_FILE_UPLOADFILE, OnFileUploadfile)
	ON_UPDATE_COMMAND_UI(ID_FILE_UPLOADFILE, OnUpdateFileUploadfile)
	ON_COMMAND(ID_NAVIGATE_RELOADCELL, OnNavigateReloadcell)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_RELOADCELL, OnUpdateNavigateReloadcell)
	ON_COMMAND(ID_VIEW_FRAME_INFO, OnViewFrameInfo)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FRAME_INFO, OnUpdateViewFrameInfo)
	ON_COMMAND(ID_VIEW_FRAME_SOURCE, OnViewFrameSource)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FRAME_SOURCE, OnUpdateViewFrameSource)
	//}}AFX_MSG_MAP
#ifdef EDITOR
	// Open file or URL to edit on Browser menu - Opens into new windows
	ON_UPDATE_COMMAND_UI(ID_FILE_OPEN_TO_EDIT, OnUpdateFileOpen)
#endif

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGenericView drawing

void CGenericView::OnDraw(CDC* pDC)
{
}

/////////////////////////////////////////////////////////////////////////////
// CGenericView diagnostics

#ifdef _DEBUG
void CGenericView::AssertValid() const
{
	CView::AssertValid();
}

void CGenericView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGenericView message handlers

CGenericDoc *CGenericView::GetDocument()        {
	CGenericDoc *pRetval = NULL;
	if(m_pContext != NULL)  {
	pRetval = m_pContext->GetDocument();
	}

	return(pRetval);
}

void CGenericView::SetContext(CAbstractCX *pContext)    
{
	ASSERT(pContext->IsWindowContext());
	m_pContext = (CWinCX *)pContext;

#ifdef WIN32
    if (GetFrame() && GetFrame()->GetFrameWnd()) {
	long lRemExStyles;
	if (lRemExStyles = ((CGenericFrame*)GetFrame()->GetFrameWnd())->GetRemovedExStyles()) {
	    long lStyles = GetWindowLong(GetSafeHwnd(), GWL_EXSTYLE); 
	    lStyles &= ~lRemExStyles;
	    SetWindowLong(GetSafeHwnd(), GWL_EXSTYLE, lStyles);
	}
    }
#endif

}

void CGenericView::FrameClosing()       {
	//      The view is closing.

	//      Are we responsible for print preveiw?
	//      If so close it down.
	if(IsInPrintPreview() == TRUE)  {
		HDC pDC = GetContext()->GetContextDC();
		CDC *tDC = CDC::FromHandle(pDC); 
		OnEndPrintPreview(tDC, NULL, CPoint(0, 0), (CPreviewView *)GetContext()->GetFrame()->GetFrameWnd()->GetActiveView());
		GetContext()->ReleaseContextDC(pDC);
	}

	//      Get rid of the context if around.
	if(GetContext() && GetContext()->IsDestroyed() == FALSE)        {
		GetContext()->DestroyContext();
	}
}

CFrameGlue *CGenericView::GetFrame() const      {
	CFrameGlue *pRetval = NULL;
	if(m_pContext != NULL)  {
		pRetval = m_pContext->GetFrame();
	}
	return(pRetval);
}

BOOL CGenericView::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	//      Attempt to pass this off to form elements first.
	//      See who's got focus.
	CWnd *pFocus = GetFocus();
	if(pFocus != NULL)      {
		//      Make sure it's a child of us, and not a CGenericView (like us).
		//      We block on the generic view, because it is already in the message map of the frame
		//              and should get appropriately called there.
		BOOL bViewChild = IsChild(pFocus);
		BOOL bGenericView = pFocus->IsKindOf(RUNTIME_CLASS(CGenericView));
		if(bViewChild == TRUE && bGenericView == FALSE) {
			//      Try an OnCmdMessage directly on the window with focus.
			//      Probably a form widget.
			//      Walk up the list of parents until we reach ourselves.
			CWnd *pTarget = pFocus;
			while(pTarget != NULL && pTarget != (CWnd *)this)       {
				if(pTarget->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
					return(TRUE);
				}
				pTarget = pTarget->GetParent();

                //  There are cases now where a child is actually a CGenericView
                //      such as the NavCenter HTML pane.  Do not allow it to
                //      receive these messages.
                if(pTarget->IsKindOf(RUNTIME_CLASS(CGenericView))) {
                    pTarget = NULL;
                }
			}
		}
	}

	//      Proceed as normal.
	return CView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CGenericView::OnActivateView(BOOL bActivate, CView *pActivateView, CView *pDeactivateView) 
{
	//      Tell the frame what the new acitve context is.  
	//      See if we should update the frame's idea of the currently active context.
	//      Don't set the active context to be absent, someone else needs to become
	//              active immediately if there is a gap.
	//      If we do clear it, things like print and print preview won't work.
	if(bActivate == TRUE)   {
		//      Frame may be gone (WM_ENDSESSION bug fix).
		if(GetFrame())  {
			GetFrame()->SetActiveContext(GetContext());
		}
	}

	//      If we're being deactivated, do so now.
	if(bActivate == FALSE)  {
		if(GetContext())        {
			GetContext()->ActivateCX(FALSE);
		}
	}

	//      Call the base.
	CView::OnActivateView(bActivate, pActivateView, pDeactivateView);

	//      If we've been activated, reflect this now.
	if(bActivate == TRUE)   {
		if(GetContext())        {
			GetContext()->ActivateCX(TRUE);
		}
    }
}

void CGenericView::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	CView::OnLButtonDblClk(nFlags, point);

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
		GetContext()->OnLButtonDblClkCX(nFlags, point);
	}
}

void CGenericView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	//      Clear current focus owner.
	//      This will get set correctly later on in the call chain when a window
	//              get's activated.
	m_hWndFocus = NULL;

	//      Call the base.
	CView::OnLButtonDown(nFlags, point);

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
		GetContext()->OnLButtonDownCX(nFlags, point);
	}
}

void CGenericView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CView::OnLButtonUp(nFlags, point);

	//      If focus didn't go to one of the children, this will give it back
	//              to the frame so that our command keys work.
	if(GetFrame() != NULL)  {
		if(GetFrame()->GetFrameWnd() != NULL)   {
            CPoint ptScreen = point;
            ClientToScreen( &ptScreen );
            HWND hWndPt = ::WindowFromPoint( ptScreen );
            if( ::IsChild( GetFrame()->GetFrameWnd()->m_hWnd, hWndPt ) || (GetFrame()->GetFrameWnd()->m_hWnd == hWndPt) )   {
    			GetFrame()->GetFrameWnd()->SetFocus();
            }
		}
	}

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
        BOOL bReturnImmediately = FALSE;
		GetContext()->OnLButtonUpCX(nFlags, point, bReturnImmediately);
        if(bReturnImmediately)  {
            return;
        }
	}
}

void CGenericView::OnMouseMove(UINT nFlags, CPoint point) 
{
	CView::OnMouseMove(nFlags, point);

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
        BOOL bReturnImmediately = FALSE;
		GetContext()->OnMouseMoveCX(nFlags, point, bReturnImmediately);
        if(bReturnImmediately)  {
            return;
        }
	}
}

void CGenericView::OnRButtonDblClk(UINT nFlags, CPoint point) 
{
	CView::OnRButtonDblClk(nFlags, point);

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
		GetContext()->OnRButtonDblClkCX(nFlags, point);
	}
}

void CGenericView::OnRButtonDown(UINT nFlags, CPoint point) 
{
    //  Force ourselves as active.
    //  Bug fix 61140, CGenFrame::OnCmdMsg doesn't know where to send
    //      resultant message.
    CFrameWnd *pFrame = NULL;
    if(GetFrame())  {
        pFrame = GetFrame()->GetFrameWnd();
    }
    if(pFrame && pFrame->IsChild(this)) {
        pFrame->SetActiveView(this);
    }

	CView::OnRButtonDown(nFlags, point);

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
		GetContext()->OnRButtonDownCX(nFlags, point);
	}
}

void CGenericView::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CView::OnRButtonUp(nFlags, point);

	//      Pass it off to the context to handle.
	if(GetContext() != NULL && GetContext()->IsDestroyed() == FALSE)        {
		GetContext()->OnRButtonUpCX(nFlags, point);
	}
}
int CGenericView::OnMouseActivate( CWnd *pWin, UINT uHitTest, UINT uMessage )
{
	if(GetContext() && GetContext()->IsDestroyed() == FALSE)        {
        if(::GetCursor() == (theApp.LoadCursor(IDC_SELECTANCHOR)))     {
            // Prevent the frame from becoming active while dragging
            return MA_NOACTIVATE;
        }
	}

	return CView::OnMouseActivate(pWin, uHitTest, uMessage);
}

//
// Form element background colors
// 
HBRUSH CGenericView::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{

    if(!GetContext() || GetContext()->IsDestroyed())
	return NULL;

    // only color buttons
    if((nCtlColor != CTLCOLOR_BTN) && (nCtlColor != CTLCOLOR_STATIC))
	return(CWnd::OnCtlColor(pDC, pWnd, nCtlColor ) );

    CWnd::OnCtlColor(pDC, pWnd, nCtlColor);

	// Assume the document background color
    COLORREF rgbCurrentColor = GetContext()->m_rgbBackgroundColor;

	// We need to use the background associated with the form element if there is one
	// (e.g. a form element in a table cell with a specified background color)
	if (pWnd->IsKindOf(RUNTIME_CLASS(CNetscapeButton))) {
		LO_FormElementStruct*   pElement = ((CNetscapeButton*)pWnd)->GetElement();

		if (pElement && pElement->text_attr && pElement->text_attr->no_background == FALSE)
			rgbCurrentColor = RGB(pElement->text_attr->bg.red, pElement->text_attr->bg.green, pElement->text_attr->bg.blue);
	}

    if(m_hCtlBrush == NULL) {
	// was no brush, just make a new one
	m_hCtlBrush = ::CreateSolidBrush(0x02000000L | rgbCurrentColor);
	m_rgbBrushColor = rgbCurrentColor;
    } else if(m_rgbBrushColor != rgbCurrentColor) {
	// old brush existed but was a different color -- destroy and remake
	DeleteObject(m_hCtlBrush);
	m_hCtlBrush = ::CreateSolidBrush(0x02000000L | rgbCurrentColor);
	m_rgbBrushColor = rgbCurrentColor;
    }

    HPALETTE hOldPal = NULL;
    if(GetContext()->GetPalette())  {
        hOldPal = ::SelectPalette(pDC->GetSafeHdc(), GetContext()->GetPalette(), FALSE);
    }
    pDC->SetBkMode(TRANSPARENT);
    pDC->SetBkColor(0x02000000L | rgbCurrentColor);
// XXX WHS Look into this more
//  if(hOldPal) {
//	    ::SelectPalette(pDC->GetSafeHdc(), hOldPal, FALSE);
//  }

    return(m_hCtlBrush);
}


#ifndef MOZ_MAIL_NEWS
extern "C" void DoAltMailComposition(MWContext *pContext);
#endif // MOZ_MAIL_NEWS

void CGenericView::OnFileMailto() 
{
	// Pass this off to the context for handling.
#ifndef MOZ_MAIL_NEWS
    DoAltMailComposition(GetContext()->GetContext());
#else /* MOZ_MAIL_NEWS */
	if(GetContext())        {
		GetContext()->MailDocument();
	}
#endif /* MOZ_MAIL_NEWS */
}

void CGenericView::OnUpdateFileMailto(CCmdUI* pCmdUI) 
{
	if(GetContext())        {
		if(GetContext()->IsGridParent() || GetContext()->IsGridCell())  {
			pCmdUI->SetText(szLoadString(IDS_MAIL_FRAME));
		}
		else    {
			pCmdUI->SetText(szLoadString(IDS_MAIL_NOFRAME));
		}
		pCmdUI->Enable(GetContext()->CanMailDocument());
	}
	else    {
		pCmdUI->Enable(FALSE);  
	}
}

// Opens a file to browse - currently called only from browser,
//   but will be called from editor in future
// Context figures out which and opens in new window  
//   only if called from and editor
void CGenericView::OnFileOpen() 
{
	if(GetContext())        {
		GetContext()->OpenFile();
	}
}

void CGenericView::OnUpdateFileOpen(CCmdUI* pCmdUI) 
{
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanOpenFile());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnNetscapeSaveAs() 
{
	if(GetContext())        {
		GetContext()->SaveAs();
	}
}

void CGenericView::OnUpdateNetscapeSaveAs(CCmdUI* pCmdUI) 
{
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanSaveAs());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnNetscapeSaveFrameAs() 
{
	if(GetContext())        {
		GetContext()->SaveAs();
	}
}

void CGenericView::OnUpdateNetscapeSaveFrameAs(CCmdUI* pCmdUI) 
{
	if(GetContext())        {
		if(GetContext()->IsGridParent() || GetContext()->IsGridCell())  {
			pCmdUI->Enable(GetContext()->CanSaveAs());
		}
		else
		{
			pCmdUI->Enable(FALSE);
		}
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnNavigateBack() 
{
	//      Let the context have it.
	if(GetContext())        {

		GetContext()->ResetToolTipImg();
		GetContext()->AllBack();

	}
}

void CGenericView::OnUpdateNavigateBack(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanAllBack());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnNavigateForward() 
{
	//      Let the context have it.
	if(GetContext())        {

		GetContext()->ResetToolTipImg();
		GetContext()->AllForward();

	}
}

void CGenericView::OnUpdateNavigateForward(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanAllForward());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnNavigateReload() 
{
	//      Let the context have it.
	if(GetContext())        {
		GetContext()->ResetToolTipImg();
        if(::GetKeyState(VK_SHIFT) & 0x8000) {
            //  Shift key was down, do it up baby.
		    GetContext()->AllReload(NET_SUPER_RELOAD);
        }
        else    {
		    GetContext()->AllReload();
        }
	}
}

void CGenericView::OnUpdateNavigateReload(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
#ifdef EDITOR
	if ( EDT_IS_EDITOR((GetContext()->GetContext())) ) {
	    // Don't allow reload for an unsaved new document 
		pCmdUI->Enable(!EDT_IS_NEW_DOCUMENT((GetContext()->GetContext())) && 
				GetContext()->CanAllReload());
	} else
#endif
		pCmdUI->Enable(GetContext()->CanAllReload());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnViewLoadimages() 
{
	//      Let the context have it.
	if(GetContext())        {
		GetContext()->ViewImages();
	}
}

void CGenericView::OnUpdateViewLoadimages(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanViewImages());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnFilePrint() 
{
	//  Let the context have it.
    //  Make sure it's the active context which gets this.
    //  In print preview of a frame cell, the command goes to the
    //      parent view, which isn't able to print, so route it manually....
	if(GetFrame() && GetFrame()->GetActiveWinContext() && GetFrame()->GetActiveWinContext()->CanPrint())    {
	    // if there is a full page plugin on the page, there can't be anything else
	    // on the page so just tell it to print whatever it wants, using it's own
	    // print context dialog.  We pass NULL as a parameter today, but in the
	    // future, we should pass in at least the printer chosen by the user.
	    if(GetFrame()->GetActiveWinContext()->ContainsFullPagePlugin()) {
		    // there can be only one plugin if it is full page
		    NPL_Print(GetFrame()->GetActiveWinContext()->GetContext()->pluginList, NULL);
			return;
	    }

		GetFrame()->GetActiveWinContext()->Print();
	}
}

void CGenericView::OnUpdateFilePrint(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {

            // Only change the text for menu items...

            if (pCmdUI->m_pMenu) {
				MWContextType type = GetContext()->GetContext() ? 
									 GetContext()->GetContext()->type : 
									 MWContextBrowser;
				
				// Only change the menu for browser windows
				if (type == MWContextBrowser) {
					//      Need to change the menu item depending on wether or not we are a frame.
					if(GetContext()->IsGridCell() || GetContext()->IsGridParent())  {
						pCmdUI->SetText(szLoadString(IDS_PRINT_FRAME));
					}
					else    {
						pCmdUI->SetText(szLoadString(IDS_PRINT_NOFRAME));
					}
				}
            }
		pCmdUI->Enable(GetContext()->CanPrint());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnUpdateFilePrintPreview(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		// Don't enable print preview for full-page plugins, because it isn't
		// currently supported (i.e. there's no routine in the plugin API to ask
		// the plugin to do something sensible)
		pCmdUI->Enable(!GetContext()->ContainsFullPagePlugin() && GetContext()->CanPrint(TRUE));
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnEditFindincurrent() 
{
	//      Let the context have it.
	if(GetContext())        {
		GetContext()->AllFind();
	}
}

void CGenericView::OnUpdateEditFindincurrent(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanAllFind());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnUpdateEditWithFrameFindincurrent(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext()) 
	{
		if(pCmdUI->m_pMenu)
			pCmdUI->m_pMenu->ModifyMenu(CASTUINT(ID_EDIT_WITHFRAME_FINDINCURRENT), CASTUINT(MF_BYCOMMAND | MF_STRING),
										CASTUINT(ID_EDIT_WITHFRAME_FINDINCURRENT), szLoadString(CASTUINT(GetContext()->IsGridCell() ? IDS_FINDINFRAME : IDS_FINDINPAGE)));

		pCmdUI->Enable(!GetContext()->IsGridParent() && GetContext()->CanAllFind());
		return;
	}

	pCmdUI->Enable(FALSE);
}

void CGenericView::OnEditFindAgain() 
{
	//      Let the context have it.
	if(GetContext()) {
		GetContext()->FindAgain();
	}
}

void CGenericView::OnUpdateEditFindAgain(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext()) {
		pCmdUI->Enable(GetContext()->CanFindAgain());
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//
// Someone has requested a find operation
//
LRESULT CGenericView::OnFindReplace(WPARAM wParam, LPARAM lParam) 
{  

	CFindReplaceDialog * dlg = ::CFindReplaceDialog::GetNotifier(lParam);
	if (!dlg) 
		return NULL;
	  
	FINDREPLACE findstruct = dlg->m_fr;
	    
	if (dlg->IsTerminating()) {
//		dlg->DestroyWindow();
		return NULL;
	}     

	MWContext *pSearchContext = ((CNetscapeFindReplaceDialog*)dlg)->GetSearchContext();

#ifdef EDITOR
    BOOL bReplaceAll = (BOOL)(findstruct.Flags & FR_REPLACEALL);
    // If no search context set, use current window's
    if( !pSearchContext )
    {
        if( GetContext() )
            pSearchContext = GetContext()->GetContext();

        if( !pSearchContext )
            return NULL;
    }
    BOOL bEditor = EDT_IS_EDITOR(pSearchContext);
    // Ignore everything except FINDNEXT if not a Composer
    if( !bEditor && ((findstruct.Flags & FR_FINDNEXT) == 0) )
        return NULL;
#else
	// Something wrong or user cancelled dialog box
	if((findstruct.Flags & FR_FINDNEXT) == 0)
		return NULL;
#endif

	
	CWinCX *pCX;

	// can only find if we have a window
	if (pSearchContext && ABSTRACTCX(pSearchContext)->IsFrameContext())	{
	    pCX = WINCX(pSearchContext);
	}
	else 
	    pCX = GetContext();

    if(pCX) {
#ifdef EDITOR
	    if(bEditor && (findstruct.Flags & FR_REPLACE) || bReplaceAll)
        {
            // Only Composer currently replaces text
            // TODO: NEED TO FINISH GETTING CORRECT PARAMS: CAN WE DO DIRECTION OR WRAP IN WINDOWS?
            EDT_ReplaceText(pSearchContext, findstruct.lpstrReplaceWith, bReplaceAll,
                            findstruct.lpstrFindWhat, !dlg->MatchCase() /* bCaseless */, 
                            !dlg->SearchDown() /*bBackward*/, FALSE /*bDoWrap*/);
            // Continue to search for next string unless we replaced all
            if( bReplaceAll )
                return TRUE;
        }
#endif
        // remember this string for next time
        theApp.m_csFindString = findstruct.lpstrFindWhat;
        theApp.m_csReplaceString = findstruct.lpstrReplaceWith;
        theApp.m_bMatchCase   = dlg->MatchCase();
        theApp.m_bSearchDown  = dlg->SearchDown(); 

        // this will pull the values we just set out of theApp and do the find
        //   operation for us
        pCX->DoFind(dlg, findstruct.lpstrFindWhat, dlg->MatchCase(), dlg->SearchDown(), TRUE);
    }
			     
	return(TRUE);
}  

void CGenericView::OnNavigateInterrupt() 
{
	//      Let the context have it.
	if(GetContext())        {
		GetContext()->AllInterrupt();
	}
}

void CGenericView::OnUpdateNavigateInterrupt(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanAllInterrupt());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnEditCopy() 
{
	//      Let the context have it.
	if(GetContext())        {
        SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
		GetContext()->CopySelection();
        SetCursor(theApp.LoadStandardCursor(IDC_ARROW));
	}
}

void CGenericView::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanCopySelection());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	//      Disable until supported.
	pCmdUI->Enable(FALSE);  
}

void CGenericView::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	//      Disable until supported.
	pCmdUI->Enable(FALSE);  
}

void CGenericView::OnSelectAll() 
{
    LO_SelectAll(GetContext()->GetDocumentContext());
}

//#ifndef NO_TAB_NAVIGATION
// BOOL CGenericView::procTabNavigation( MSG * pMsg )
BOOL CGenericView::procTabNavigation(UINT nChar, UINT forward, UINT controlKey ) 
{
	//      Call the base.
	// BOOL    bTranslated = CView::PreTranslateMessage(pMsg);

	//if ( bTranslated || pMsg->message != WM_KEYDOWN)
	//	return( bTranslated );
	

	if(	nChar != VK_TAB && nChar != VK_SPACE && nChar != VK_RETURN	)
		return( FALSE );

	if( GetContext() == NULL || GetContext()->IsDestroyed() == TRUE)
		return( FALSE );

	BOOL	ret;
	if( nChar == VK_TAB ) {
		// setTabFocusNext will search siblings
		ret = GetContext()->setTabFocusNext( forward );	// 1 is forward
		return( ret );		// ret == TRUE if key handled by me or by one of my sibling
	} 

	if (  nChar == VK_RETURN ) {			// nChar == VK_SPACE ||
		// fire action for links only. 
		// Form elements will handle this in their OnChar
		ret = GetContext()->fireTabFocusElement( nChar );
		return( ret );		// ret == TRUE if I fired a link.
	}

    // should not reach here
	return( FALSE );
}	// BOOL CGenericView::procTabNavigation( MSG * pMsg )
//#endif	/* NO_TAB_NAVIGATION */

BOOL CGenericView::PreTranslateMessage(MSG * pMsg)
{
    return(FALSE);
}

void CGenericView::OnFileViewsource() 
{
	//      Have the context handle it.
	if(GetContext())        {
		GetContext()->ViewSource();
	}
}

void CGenericView::OnUpdateFileViewsource(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanViewSource());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}       
}

void CGenericView::OnFileDocinfo() 
{
	//      Have the context handle it.
	if(GetContext())        {
		GetContext()->DocumentInfo();
	}
}

void CGenericView::OnUpdateFileDocinfo(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanDocumentInfo());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}       
}

void CGenericView::OnViewPageServices() 
{
    URL_Struct *pNewUrl_s;

	char *url;

	if(GetContext() && GetContext()->GetContext())
	{
		if(SHIST_CurrentHandlesPageServices(GetContext()->GetContext()))
		{
			url = SHIST_GetCurrentPageServicesURL(GetContext()->GetContext());

			if(url)
			{
			    pNewUrl_s = NET_CreateURLStruct(url, NET_DONT_RELOAD);
				GetContext()->GetUrl(pNewUrl_s, FO_PRESENT);
			}
		}
	}	

}

void CGenericView::OnUpdateViewPageServices(CCmdUI* pCmdUI) 
{
	if(GetContext() && GetContext()->GetContext())
	{
		pCmdUI->Enable(SHIST_CurrentHandlesPageServices(GetContext()->GetContext()));
	}
	else
		pCmdUI->Enable(FALSE);

}

void CGenericView::OnGoHome() 
{
	//      Have the context handle it.
	if(GetContext())        {
		GetContext()->GoHome();
	}
}

void CGenericView::OnUpdateGoHome(CCmdUI* pCmdUI) 
{
	//      Defer to the context.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanGoHome());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnMove(int x, int y) 
{
	CView::OnMove(x, y);
	
	if(GetContext())        {
		GetContext()->OnMoveCX();
	}
}

void CGenericView::OnNcPaint()
{
	//      Call the base (it will handle scrollers and other stuff).
	CView::OnNcPaint();

	//      Now, tell the context to do anything it needs to do.
	if(GetContext())        {
		GetContext()->OnNcPaintCX();
	}
}

void CGenericView::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
    //  Call the base (it will fill in all the normal stuff).
    CView::OnNcCalcSize(bCalcValidRects, lpncsp);

    //  Now, have the context shrink us if appropriate.
    if(GetContext())    {
        GetContext()->OnNcCalcSizeCX(bCalcValidRects, lpncsp);
    }
}


void CGenericView::OnFileUploadfile() 
{
	//      Have the context handle it.
	if(GetContext())        {
		GetContext()->UploadFile();
	}
}

void CGenericView::OnUpdateFileUploadfile(CCmdUI* pCmdUI) 
{
	//      Defer to the context.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->CanUploadFile());
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnNavigateReloadcell() 
{
	//      Have the current context reload.
	if(GetContext())        {
        if(::GetKeyState(VK_SHIFT) & 0x8000) {
            //  Shift key was down, do the super thing.
		    GetContext()->Reload(NET_SUPER_RELOAD);
        }
        else    {
		    GetContext()->Reload();
        }
	}
}

void CGenericView::OnUpdateNavigateReloadcell(CCmdUI* pCmdUI) 
{
	//      Defer to the context's wisdom.
	if(GetContext())        {
		pCmdUI->Enable(GetContext()->IsGridCell() == TRUE
			&& GetContext()->IsGridParent() == FALSE);
	}
	else    {
		pCmdUI->Enable(FALSE);
	}
}

void CGenericView::OnViewFrameInfo() 
{
    if(GetContext())    {
	GetContext()->FrameInfo();
    }
}

void CGenericView::OnUpdateViewFrameInfo(CCmdUI* pCmdUI) 
{
    //  Defer to the context
    if(GetContext())    {
	pCmdUI->Enable(GetContext()->CanFrameInfo());
    }
    else    {
	pCmdUI->Enable(FALSE);
    }
}

void CGenericView::OnViewFrameSource() 
{
    if(GetContext())    {
	GetContext()->FrameSource();
    }
}

void CGenericView::OnUpdateViewFrameSource(CCmdUI* pCmdUI) 
{
    //  Defer to the context
    if(GetContext())    {
	pCmdUI->Enable(GetContext()->CanFrameSource());
    }
    else    {
	pCmdUI->Enable(FALSE);
    }
}

void CGenericView::OnSetFocus(CWnd* pOldWnd) 
{
	CView::OnSetFocus(pOldWnd);
    if(GetContext() && !GetContext()->IsDestroyed() && GetContext()->GetContext())    {
        // send the event to libmocha --- do any further processing
        //   in our closure routine
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_FOCUS;
        
	ET_SendEvent(GetContext()->GetContext(), NULL, event, NULL, 
                     this);
    }
}

void CGenericView::OnKillFocus(CWnd* pNewWnd) 
{
	CView::OnKillFocus(pNewWnd);
    if(GetContext() && !GetContext()->IsDestroyed() && GetContext()->GetContext())    {
        // send the event to libmocha --- do any further processing
        //   in our closure routine
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_BLUR;

        ET_SendEvent(GetContext()->GetContext(), NULL, event, NULL, 
                     this);
    }
}

