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
#include "csttlbr2.h"
#include "toolbar2.h"
#include "mainfrm.h"
#include "edcombtb.h"
      
#define IDT_DRAGTOOLBAR_DRAGGING 16415
#define DRAGTOOLBAR_DRAGGING_DELAY 100
#define IDT_TABHAVEFOCUS 16416
#define TABHAVEFOCUS_DELAY 100

#define CUSTTOOLBARID 33333
#define CUTOFF_ON_ANIM_RESIZE 12

#ifdef XP_WIN16
LRESULT CNetscapeControlBar::WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	ASSERT_VALID(this);

	// parent notification messages are just passed to parent of control bar
	switch (nMsg)
	{
	case WM_COMMAND:
		return CWnd::WindowProc(nMsg, wParam, lParam);
		break;
		
	case WM_DRAWITEM:
	case WM_MEASUREITEM:
	case WM_DELETEITEM:
	case WM_COMPAREITEM:
	case WM_VKEYTOITEM:
	case WM_CHARTOITEM:
	case WM_VBXEVENT:
		return GetOwner()->SendMessage(nMsg, wParam, lParam);
	}
	return CWnd::WindowProc(nMsg, wParam, lParam);
}
#endif

// Class:  CToolbarWindow
//
// The window that resides within the Tabbed toolbar.  It holds the toolbar
// passed in by the user of the customizable toolbar


// Function - CToolbarWindow
//
// Constructor
// 
// Arguments - None
//
// Returns - Nothing
CToolbarWindow::CToolbarWindow(CWnd *pToolbar, int nToolbarStyle, int nNoviceHeight, int nAdvancedHeight,
							   HTAB_BITMAP nHTab)
{
	m_pToolbar = pToolbar;
	m_nNoviceHeight = nNoviceHeight;
	m_nAdvancedHeight = nAdvancedHeight;
	m_nToolbarStyle = nToolbarStyle;
	m_nHTab = nHTab;
}

CToolbarWindow::~CToolbarWindow()
{
}

CWnd * CToolbarWindow::GetToolbar(void)
{
	
	return m_pToolbar;	
	
}

int CToolbarWindow::GetHeight(void)
{
	return(m_nToolbarStyle == TB_PICTURESANDTEXT ? m_nNoviceHeight : m_nAdvancedHeight);
}


// End of CToolbarWindow implementation

// Class:  CButtonToolbarWindow
//

CButtonToolbarWindow::CButtonToolbarWindow(CWnd *pToolbar, int nToolbarStyle, int nNoviceHeight, int nAdvancedHeight,
										   HTAB_BITMAP nHTab) 
: CToolbarWindow(pToolbar, nToolbarStyle, nNoviceHeight, nAdvancedHeight, nHTab)
{
}

void CButtonToolbarWindow::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{

	((CNSToolbar2*)GetToolbar())->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
}

void CButtonToolbarWindow::UpdateURLBars(char* url)
{
	((CNSToolbar2*)GetToolbar())->UpdateURLBars(url);
}

void CButtonToolbarWindow::SetToolbarStyle(int nToolbarStyle)
{

	CToolbarWindow::SetToolbarStyle(nToolbarStyle);
	((CNSToolbar2*)GetToolbar())->SetToolbarStyle(nToolbarStyle);

}

int CButtonToolbarWindow::GetHeight(void)
{
	return(((CNSToolbar2*)GetToolbar())->GetHeight());
}

// Class:  CControlBarToolbarWindow
//
CControlBarToolbarWindow::CControlBarToolbarWindow(CWnd *pToolbar, int nToolbarStyle, int nNoviceHeight, int nAdvancedHeight,
										   HTAB_BITMAP nHTab) 
: CToolbarWindow(pToolbar, nToolbarStyle, nNoviceHeight, nAdvancedHeight, nHTab)
{
}

// cast to CComboToolbar because these functions don't exist in Win16.  Will need a better solution later.
int CControlBarToolbarWindow::GetHeight(void)
{
/*	CRect rect;
	m_pToolbar->GetWindowRect(&rect);*/
	CSize size;

#ifdef EDITOR // jevering says this is the right ifdef here
#ifdef XP_WIN32 	
	size = ((CComboToolBar*)m_pToolbar)->CalcDynamicLayout( 32500, LM_STRETCH | LM_HORZ);
#else
	size = ((CComboToolBar*)m_pToolbar)->CalcDynamicLayout( 32500, 0);      
#endif
#endif /* EDITOR */

	return (size.cy);
}

void CControlBarToolbarWindow::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{
#ifdef MOZ_MAIL_NEWS
	((CComboToolBar*)GetToolbar())->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
#endif /* MOZ_MAIL_NEWS */   
}



#define OPEN_BUTTON_WIDTH 9
#define CLOSED_BUTTON_WIDTH 40
#define CLOSED_BUTTON_HEIGHT 10
#define DT_RIGHT_MARGIN 1		//Margin between end of toolbar and right border of Navigator
#define DRAGGING_BORDER_HEIGHT 2
#define NOTOOL -1

#define HTAB_TOP_START 0
#define HTAB_TOP_HEIGHT 7
#define HTAB_MIDDLE_START 8
#define HTAB_MIDDLE_HEIGHT 6
#define HTAB_BOTTOM_START 15
#define HTAB_BOTTOM_HEIGHT 3

HBITMAP				m_hTabBitmap = NULL;
int					nTabBitmapRefCount = 0;
//////////////////////////////////////////////////////////////////////////
//					Class CDragToolbar
//////////////////////////////////////////////////////////////////////////

CDragToolbar::CDragToolbar()
{
	m_bIsOpen = TRUE;
	m_bIsShowing = TRUE;
	m_pToolbar = NULL;
	m_bDragging = FALSE;
	m_bMouseDown = FALSE;
	m_bMouseInTab = FALSE;
	m_pAnimation = NULL;
	m_bEraseBackground = TRUE;
	m_nToolID = NOTOOL;
	

}

CDragToolbar::~CDragToolbar()
{

	if(m_pToolbar != NULL)
	{
		delete m_pToolbar;
	}

	nTabBitmapRefCount--;

	if(nTabBitmapRefCount == 0)
	{
		if(m_hTabBitmap)
			DeleteObject(m_hTabBitmap);
	}
}


int CDragToolbar::Create(CWnd *pParent, CToolbarWindow *pToolbar)
{
	CRect rect(0, 0, 0, 0);

	m_pToolbar = pToolbar;

	CBrush brush;

	DWORD shouldClipChildren = 0;
	if (ShouldClipChildren())
		shouldClipChildren = WS_CLIPCHILDREN;

	if (!CWnd::Create(theApp.NSToolBarClass, NULL, shouldClipChildren | WS_CHILD | WS_CLIPSIBLINGS, rect,
		pParent, 0))
	{
		return 0;
	}

	pToolbar->GetToolbar()->SetOwner(GetParentFrame());
	pToolbar->GetToolbar()->SetParent(this);

	m_eHTabType = pToolbar->GetHTab();

	HINSTANCE hInst = AfxGetResourceHandle();
	CDC *pDC = GetDC();

//	CGenericFrame *pGenFrame = (CGenericFrame*)GetParentFrame();
//	HPALETTE hPalette = ((CDCCX*)pGenFrame->GetMainContext())->GetPalette();

	if(nTabBitmapRefCount == 0)
	{
		WFE_InitializeUIPalette(pDC->m_hDC);
//		HPALETTE hPalette = WFE_GetUIPalette();
		m_hTabBitmap = WFE_LoadTransparentBitmap( hInst, pDC->m_hDC, sysInfo.m_clrBtnFace, RGB(255, 0, 255),
												  WFE_GetUIPalette(GetParentFrame( )), IDB_VERTICALTAB);
	}
	nTabBitmapRefCount++;

	ReleaseDC(pDC);

	m_toolTip.Create(this, TTS_ALWAYSTIP);
	CRect tipRect(0, 0, OPEN_BUTTON_WIDTH,GetToolbarHeight());
	m_toolTip.AddTool(this, m_tabTip, &tipRect,  1);
	m_toolTip.Activate(TRUE);
	m_toolTip.SetDelayTime(250);

	return 1;
}

void CDragToolbar::SetTabTip(CString tabTip)
{

	m_tabTip = tabTip;

	m_toolTip.UpdateTipText(m_tabTip, this, 1);
}

CWnd *CDragToolbar::GetToolbar(void)
{
	return m_pToolbar->GetToolbar();

}

int  CDragToolbar::GetToolbarHeight(void)
{
    int height = m_pToolbar->GetHeight();

    if(m_pAnimation != NULL)
	{
	
        CSize size;
        m_pAnimation->GetSize( size, FALSE );
                
        int diff = size.cy - height + 2;
        if (diff > 0)
        {
            if (diff > CUTOFF_ON_ANIM_RESIZE)
            {
                m_pAnimation->SetBitmap( TRUE );
                m_pAnimation->GetSize( size, TRUE );
                diff = size.cy - height + 2;

                if (diff < 0)
                    diff = 0;
            }
        }
        else diff = 0;

        height += diff;
    }

	return height;
}

void CDragToolbar::SetToolbarStyle(int nToolbarStyle)
{
	m_pToolbar->SetToolbarStyle(nToolbarStyle);
	// may need to change tool rect
	CRect tipRect(0, 0, OPEN_BUTTON_WIDTH, GetToolbarHeight());
	m_toolTip.SetToolRect(this, 1, &tipRect);

}

void CDragToolbar::SetAnimation(CAnimationBar2 *pAnimation)
{
	m_pAnimation = pAnimation;
	
    CRect rect;
	GetClientRect(&rect);

    int toolbarHeight = m_pToolbar->GetHeight();
    int tempHeight = toolbarHeight;
	
	if(m_pAnimation != NULL)
	{
		m_pAnimation->SetOwner(this);
		m_pAnimation->SetParent(this);

        CSize size;
        pAnimation->GetSize( size, FALSE );
                
        int diff = size.cy - toolbarHeight + 2;
       
        if (diff > 0)
        {
            if (diff > CUTOFF_ON_ANIM_RESIZE)
            {
                pAnimation->SetBitmap( TRUE ); // Small bitmap
                
                pAnimation->GetSize( size, TRUE );
                
                diff = size.cy - toolbarHeight + 2;
                if (diff < 0)  // Resize to accommodate small bitmap if necessary
                    diff = 0; 
            }
            else pAnimation->SetBitmap( FALSE );
            
			if (diff > 0)
			{
				toolbarHeight += diff;
				SetWindowPos(NULL, 0, 0, rect.Width(), toolbarHeight, SWP_NOMOVE | SWP_NOZORDER);
				GetParentFrame()->RecalcLayout();
				
				if (m_bDragging)
				{
					SetWindowPos(NULL, 0, 0, rect.Width(), toolbarHeight + DRAGGING_BORDER_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
				}
				
			}
		}
        else pAnimation->SetBitmap( FALSE ); // Big bitmap

        // The toolbar's width will change.  This could affect its height.

		CNSToolbar2* theBar = (CNSToolbar2*)(m_pToolbar->GetNSToolbar());
		if (theBar)
          theBar->WidthChanged(size.cx);

        // If the toolbar's height INCREASED, then perhaps the large animation will now fit.
        // Simply perform the check all over again.
        if (m_pToolbar->GetHeight() > tempHeight)
        {
            SetAnimation(pAnimation);
            return;
        }
	}
	
	ArrangeToolbar(rect.Width(), toolbarHeight + (m_bDragging ? DRAGGING_BORDER_HEIGHT : 0));
	RedrawWindow();
}

void CDragToolbar::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{

	m_pToolbar->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
}

void CDragToolbar::UpdateURLBars(char* url)
{
	m_pToolbar->UpdateURLBars(url);
}

//////////////////////////////////////////////////////////////////////////
//					Messages for CDragToolbar
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CDragToolbar, CWnd)
	//{{AFX_MSG_MAP(CControlBar)
    ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	ON_WM_PALETTECHANGED()
	ON_WM_SYSCOLORCHANGE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CDragToolbar::OnSize( UINT nType, int cx, int cy )
{
	if(m_pToolbar)
	{
		ArrangeToolbar(cx, cy);
	}
}

void CDragToolbar::OnPaint(void)
{

	CPaintDC dcPaint(this);	// device context for painting
	CRect rect;

	GetClientRect(&rect);

	;
	COLORREF rgbOldBk = ::SetBkColor(dcPaint.m_hDC, sysInfo.m_clrBtnFace);

	// Use our background color
	HBRUSH brFace = sysInfo.m_hbrBtnFace;
	::FillRect(dcPaint.m_hDC, &rect, brFace);

	CDC *pDC = &dcPaint;
	HPALETTE hOldPal = ::SelectPalette(pDC->m_hDC, WFE_GetUIPalette(GetParentFrame()), FALSE);

	if(m_hTabBitmap != NULL)
	{

		// Create a scratch DC and select our bitmap into it.
		CDC * pBmpDC = new CDC;
		pBmpDC->CreateCompatibleDC(pDC);


		HBITMAP hOldBmp = (HBITMAP)::SelectObject(pBmpDC->m_hDC ,m_hTabBitmap);
		HPALETTE hOldPalette = ::SelectPalette(pBmpDC->m_hDC, WFE_GetUIPalette(NULL), TRUE);
		::RealizePalette(pBmpDC->m_hDC);
		CPoint bitmapStart(!m_bMouseInTab ? 0 : OPEN_BUTTON_WIDTH ,HTAB_TOP_START);

		//First do top of the tab

		::BitBlt(pDC->m_hDC, 0, 0, OPEN_BUTTON_WIDTH, HTAB_TOP_HEIGHT,
	  			 pBmpDC->m_hDC, bitmapStart.x, bitmapStart.y, SRCCOPY);
	
		//Now do the middle portion of the tab
		int y = HTAB_TOP_HEIGHT;

		bitmapStart.y = HTAB_MIDDLE_START;

		while(y < rect.bottom - HTAB_BOTTOM_HEIGHT)
		{
			::BitBlt(pDC->m_hDC, 0, y, OPEN_BUTTON_WIDTH, HTAB_MIDDLE_HEIGHT,
		  			 pBmpDC->m_hDC, bitmapStart.x, bitmapStart.y, SRCCOPY);

			y += HTAB_MIDDLE_HEIGHT;

		}

		// Now do the bottom of the tab
		y = rect.bottom - HTAB_BOTTOM_HEIGHT;

		bitmapStart.y = HTAB_BOTTOM_START;

		::BitBlt(pDC->m_hDC, 0, y, OPEN_BUTTON_WIDTH, HTAB_BOTTOM_HEIGHT,
	  			 pBmpDC->m_hDC, bitmapStart.x, bitmapStart.y, SRCCOPY);


		// Cleanup
		::SelectObject(pBmpDC->m_hDC, hOldBmp);
		::SelectPalette(pBmpDC->m_hDC, hOldPalette, TRUE);
		::SelectPalette(pDC->m_hDC, hOldPal, TRUE);
		pBmpDC->DeleteDC();
		delete pBmpDC;
	}

	if(m_bDragging)
	{
		CBrush brush(RGB(0, 0, 0));
		CBrush *pOldBrush = (CBrush*)dcPaint.SelectObject(&brush);

		//rect.left += OPEN_BUTTON_WIDTH;

		dcPaint.FrameRect(&rect, &brush);

		dcPaint.SelectObject(pOldBrush);
		brush.DeleteObject();
	}
	::SetBkColor(dcPaint.m_hDC, rgbOldBk);

}

void CDragToolbar::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CDragToolbar::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}

void CDragToolbar::OnLButtonDown(UINT nFlags, CPoint point)
{
	MSG msg = *(GetCurrentMessage());
	m_toolTip.RelayEvent(&msg);

	m_mouseDownPoint = point;
	m_bMouseDown = TRUE;
	//this doesn't mean we are going to drag, just that if we start moving we are dragging
	CWnd::OnLButtonDown(nFlags, point);

}

void CDragToolbar::OnLButtonUp(UINT nFlags, CPoint point)
{

	MSG msg = *(GetCurrentMessage());
	m_toolTip.RelayEvent(&msg);

	if(m_bDragging)
	{
		StopDragging();
	}
	else if(m_bMouseDown && point.x >= 0 && point.x <= OPEN_BUTTON_WIDTH)
	{
		GetParent()->SendMessage(CT_HIDETOOLBAR, 0, (LPARAM)m_hWnd);
	}
	m_bMouseDown = FALSE;

	//ReleaseCapture();
}

void CDragToolbar::OnMouseMove(UINT nFlags, CPoint point)
{

	if(nFlags & MK_LBUTTON)
	{
	//	if(!GetCapture())
	//		SetCapture();
		
		if(m_bDragging || (m_bMouseDown && (abs(m_mouseDownPoint.x - point.x) > 3 || abs(m_mouseDownPoint.y - point.y) > 3)))
		{
			if(!m_bDragging)
			{
				CRect clientRect;
				GetClientRect(&clientRect);
				MapWindowPoints(GetParent(), clientRect);
				m_bDragging =TRUE;	
				m_nDragTimer = SetTimer(IDT_DRAGTOOLBAR_DRAGGING, DRAGTOOLBAR_DRAGGING_DELAY, NULL);
				//Create dragging border
				MoveWindow(clientRect.left, clientRect.top, clientRect.Width(), clientRect.Height() + DRAGGING_BORDER_HEIGHT);

				RedrawWindow();
			}

			CPoint diffPoint;

			diffPoint.x =  (point.x - m_mouseDownPoint.x);
			diffPoint.y =  (point.y - m_mouseDownPoint.y);

			MapWindowPoints(GetParent(), &diffPoint, 1);
			
			CRect parentRect;
			GetParent()->GetWindowRect(&parentRect);
		
			GetCursorPos(&point);
			
			if(point.x - m_mouseDownPoint.x < parentRect.left)
				diffPoint.x = 0;

			if(point.y - m_mouseDownPoint.y < parentRect.top)
				diffPoint.y = 0;
		
			if(parentRect.PtInRect(point))
			{
				GetParent()->SendMessage(CT_DRAGTOOLBAR, (WPARAM) m_hWnd, MAKELPARAM(diffPoint.x, diffPoint.y));
			}
			else
			{
				//if we aren't in the rect, we will treat this like a mouse up.
				StopDragging();
			}
		}
	}
	else
	{
		m_toolTip.Activate(TRUE);
		MSG msg = *(GetCurrentMessage());
		m_toolTip.RelayEvent(&msg);

	}

	CheckIfMouseInTab(point);
}

void CDragToolbar::OnTimer( UINT  nIDEvent )
{

	CRect rect;
	GetWindowRect(&rect);
	POINT pt;
	GetCursorPos(&pt);

	if(nIDEvent == IDT_DRAGTOOLBAR_DRAGGING)
	{

		//Give some room on the left border
		rect.left -=5;
		BOOL bMouseInToolbar = PtInRect(&rect, pt);

		SHORT sMouseState;
		BOOL bMouseDown;

		//check to see if the mouse is up.  If it is, we need to go back in place.
		if(!GetSystemMetrics(SM_SWAPBUTTON))
			sMouseState = GetAsyncKeyState(VK_LBUTTON);
		else
			sMouseState = GetAsyncKeyState(VK_RBUTTON);

		bMouseDown = sMouseState & 0x8000;

		if((m_bDragging && !bMouseInToolbar) || (m_bDragging && !bMouseDown))
		{
			m_bMouseDown = FALSE;
			StopDragging();
			KillTimer(m_nDragTimer);
		}
	}
	else if(nIDEvent == IDT_TABHAVEFOCUS)
	{
		rect.right = rect.left + OPEN_BUTTON_WIDTH;
		BOOL bMouseInTab = PtInRect(&rect, pt);		
		if(m_bMouseInTab && !bMouseInTab)
		{
			ScreenToClient(&rect);
			KillTimer(m_nTabFocusTimer);
			m_bMouseInTab = FALSE;
			InvalidateRect(&rect, FALSE);
		}
	}
	CWnd::OnTimer(nIDEvent);

}

void CDragToolbar::OnPaletteChanged( CWnd* pFocusWnd )
{
	Invalidate();

}

void CDragToolbar::OnSysColorChange( )
{
	if(nTabBitmapRefCount > 0)
	{
		if(m_hTabBitmap)
			DeleteObject(m_hTabBitmap);
		HINSTANCE hInst = AfxGetResourceHandle();
		HDC hDC = ::GetDC(m_hWnd);
		m_hTabBitmap = WFE_LoadTransparentBitmap(hInst, hDC, sysInfo.m_clrBtnFace, 
			RGB(255, 0, 255), WFE_GetUIPalette(GetParentFrame()), IDB_VERTICALTAB);
			
		::ReleaseDC(m_hWnd, hDC);
		Invalidate();
	}
}


//////////////////////////////////////////////////////////////////////////
//					CDragToolbar Helpers
//////////////////////////////////////////////////////////////////////////

void CDragToolbar::ArrangeToolbar(int nWidth, int nHeight)
{
    if (nHeight == 0)
        return;

	CRect animRect, clientRect;
   
    int toolbarHeight = m_pToolbar->GetHeight();    

	if(m_pAnimation != NULL)
	{
         m_pAnimation->GetClientRect(&animRect);

		m_pAnimation->SetWindowPos(NULL, nWidth - animRect.Width() - DT_RIGHT_MARGIN, (nHeight - animRect.Height()) / 2, animRect.Width(), animRect.Height(), 
									 SWP_NOZORDER|SWP_NOSIZE);
		nWidth -=animRect.Width();
	}

    m_pToolbar->GetToolbar()->MoveWindow(OPEN_BUTTON_WIDTH, (nHeight - toolbarHeight) / 2, nWidth - OPEN_BUTTON_WIDTH - DT_RIGHT_MARGIN, toolbarHeight);
    
	CNSToolbar2* theBar = (CNSToolbar2*)(m_pToolbar->GetNSToolbar());
	if (theBar)
        theBar->WidthChanged(m_pAnimation? animRect.Width() : 0);
}

void CDragToolbar::StopDragging(void)
{
	ReleaseCapture();
	m_bDragging = FALSE;
	CRect clientRect;
	GetClientRect(&clientRect);
	MapWindowPoints(GetParent(), clientRect);
	//Remove dragging border
	MoveWindow(clientRect.left, clientRect.top, clientRect.Width(), clientRect.Height() - DRAGGING_BORDER_HEIGHT);
	RedrawWindow();
	GetParent()->SendMessage(CT_DRAGTOOLBAR_OVER, 0, (LPARAM)m_hWnd);
}

void CDragToolbar::CheckIfMouseInTab(CPoint point)
{
	if(!m_bMouseInTab)
	{
		CRect clientRect, clientInWinRect;
		GetClientRect(&clientRect);
		clientRect.right = OPEN_BUTTON_WIDTH;
		clientInWinRect = clientRect;
		ClientToScreen(&clientInWinRect);
		//Make consistent with dragging
		clientInWinRect.left -=5;
		ClientToScreen(&point);
		if(PtInRect(&clientInWinRect, point))
		{
			m_bMouseInTab = TRUE;
			m_nTabFocusTimer = SetTimer(IDT_TABHAVEFOCUS, TABHAVEFOCUS_DELAY, NULL);
			InvalidateRect(clientRect, FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//					Class  CCustToolbarExternalTab
//////////////////////////////////////////////////////////////////////////

CCustToolbarExternalTab::CCustToolbarExternalTab(CWnd *pOwner, HTAB_BITMAP eHTabType, UINT nTabTip, UINT nTabID)
{
	m_pOwner = pOwner;
	m_eHTabType = eHTabType;
	m_tabTip.LoadString(nTabTip);
	m_nTabID = nTabID;
}

CWnd *CCustToolbarExternalTab::GetOwner(void)
{
	return m_pOwner;
}

HTAB_BITMAP CCustToolbarExternalTab::GetHTabType(void)
{
	return m_eHTabType;
}

CString & CCustToolbarExternalTab::GetTabTip (void)
{ 
	return m_tabTip;
}

UINT CCustToolbarExternalTab::GetTabID(void)
{
	return m_nTabID;
}

//////////////////////////////////////////////////////////////////////////
//					Class  CCustToolbar
//////////////////////////////////////////////////////////////////////////

#define SPACE_BETWEEN_TOOLBARS 2

CCustToolbar::CCustToolbar(int nNumToolbars)
{
	m_bEraseBackground = TRUE;
	m_nNumToolbars = nNumToolbars;
	m_pParent = NULL;
	m_nActiveToolbars = 0;
	m_nNumOpen = 0;
	m_nNumShowing = 0;
	m_pToolbarArray = new CDragToolbar*[nNumToolbars];
	m_pHiddenToolbarArray = new CDragToolbar*[nNumToolbars];
	for(int i = 0; i < nNumToolbars; i++)
	{
		m_pToolbarArray[i] = NULL;
		m_pHiddenToolbarArray[i] = NULL;
	}

	m_pAnimation = NULL;
	m_nAnimationPos = -1;
	m_oldDragPoint.x = m_oldDragPoint.y = -1;
	m_bSaveToolbarInfo = TRUE;
	m_nTabHaveFocusTimer = 0;
	m_nMouseOverTab = -1;
	m_bBottomBorder = FALSE;
}

CCustToolbar::~CCustToolbar()
{
	int i;

	for(i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL)
		{
			delete m_pToolbarArray[i];
		}

		if(m_pHiddenToolbarArray[i] != NULL)
		{
			delete m_pHiddenToolbarArray[i];
		}
	}
	delete [] m_pToolbarArray;
	delete [] m_pHiddenToolbarArray;

	//delete everything in external tabl array
	int nExtTabSize = m_externalTabArray.GetSize();
	for(i = 0; i < nExtTabSize; i++)
	{
		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab*)m_externalTabArray[i];
		delete pExtTab;
	}

	m_externalTabArray.RemoveAll();

	for(i = 0; i < 4; i++)
	{
		if(m_pHorizTabArray[i] )
		{
			::DeleteObject(m_pHorizTabArray[i]);
			m_pHorizTabArray[i] = NULL;
		}
	}

	if(m_pAnimation)
		delete m_pAnimation;


}


int CCustToolbar::Create(CFrameWnd* pParent, BOOL bHasAnimation)
{
	DWORD dwStyle;
	CRect rect;

	ASSERT_VALID(pParent);   // must have a parent

//	dwStyle &= ~CBRS_ALL;
#ifdef WIN32
	dwStyle = CBRS_ALIGN_TOP|CCS_NOPARENTALIGN|CCS_NOMOVEY|CCS_NODIVIDER|CCS_NORESIZE|CBRS_SIZE_DYNAMIC;
#else
	dwStyle = CBRS_TOP; //Align top
#endif
	// create the HWND
	//rect.SetRectEmpty();
	rect.SetRect(0,0, 100, 100);
	CBrush brush;

	if (!CWnd::Create(theApp.NSToolBarClass, NULL, dwStyle | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect,
		pParent, CUSTTOOLBARID))
	{
		return 0;
	}
	// Note: Parent must resize itself for control bar to be resized

#ifdef WIN32
	SetBarStyle(CBRS_ALIGN_TOP );
	pParent->ShowControlBar(this, TRUE, FALSE);
#endif
	m_pParent=pParent;

//	CreateToolbars(m_nNumToolbars);

	if(bHasAnimation)
	{
		CRect animRect(0, 0, 45, 35);
		m_pAnimation = new CAnimationBar2;
		
		CBrush brush2;

		m_pAnimation->Create(AfxRegisterWndClass(0 ,
				theApp.LoadStandardCursor(IDC_ARROW), 
				(HBRUSH)(COLOR_BTNFACE + 1)), NULL, WS_CHILD | WS_CLIPSIBLINGS,
				 animRect, this, 0, NULL);

		m_pAnimation->ShowWindow(SW_HIDE);

	}

	m_pHorizTabArray[LARGE_FIRST] = CreateHorizTab(IDB_LARGE_HFTAB);
	m_pHorizTabArray[LARGE_OTHER] = CreateHorizTab(IDB_LARGE_HTAB);
	m_pHorizTabArray[SMALL_FIRST] = CreateHorizTab(IDB_SMALL_HFTAB);
	m_pHorizTabArray[SMALL_OTHER] = CreateHorizTab(IDB_SMALL_HTAB);

	m_toolTip.Create(this, TTS_ALWAYSTIP);
	m_toolTip.Activate(TRUE);
	m_toolTip.SetDelayTime(250);

	return 1;
}

CDragToolbar* CCustToolbar::CreateDragBar()
{
	return new CDragToolbar();
}

void CCustToolbar::AddNewWindow(UINT nToolbarID, CToolbarWindow* pWindow, int nPosition, int nNoviceHeight, int nAdvancedHeight,
								UINT nTabBitmapIndex, CString tabTip, BOOL bIsNoviceMode, BOOL bIsOpen, BOOL bIsAnimation)
{

	if(m_pToolbarArray[nPosition] != NULL || nPosition < 0 || nPosition >= m_nNumToolbars)
		nPosition = FindFirstAvailablePosition();

	CDragToolbar *pDragToolbar = CreateDragBar();

	if(pDragToolbar->Create(this, pWindow))
	{
		m_nNumShowing++;
		m_pToolbarArray[nPosition] = pDragToolbar;
		pWindow->GetToolbar()->ShowWindow(SW_SHOW);
		pDragToolbar->SetOpen(bIsOpen);
		pDragToolbar->SetTabTip(tabTip);
		pDragToolbar->SetToolbarID(nToolbarID);

		if(bIsOpen)
		{
			pDragToolbar->ShowWindow(SW_SHOW);
			m_nNumOpen++;
			if(nPosition < m_nAnimationPos || m_nAnimationPos == -1)
			{
				if(m_nAnimationPos != -1)
				{
					m_pToolbarArray[m_nAnimationPos]->SetAnimation(NULL);
				}
				m_pToolbarArray[nPosition]->SetAnimation(m_pAnimation);
				if(m_pAnimation)
					m_pAnimation->ShowWindow(SW_SHOW);
				m_nAnimationPos = nPosition;
				
			}
		}

		m_nActiveToolbars++;


		m_pParent->RecalcLayout();
	}

}

void CCustToolbar::StopAnimation(void)
{
	if(m_pAnimation)
		m_pAnimation->StopAnimation();

}

void CCustToolbar::StartAnimation()
{
	if(m_pAnimation)
		m_pAnimation->StartAnimation();
}

//This is if the window is showing, not if it is iconified
BOOL CCustToolbar::IsWindowShowing(CWnd *pToolbar)
{
	int nIndex = FindDragToolbarFromWindow(pToolbar, m_pToolbarArray);
	return (nIndex != -1);
}

BOOL CCustToolbar::IsWindowShowing(UINT nToolbarID)
{
	int nIndex = FindDragToolbarFromID(nToolbarID, m_pToolbarArray);
	return (nIndex != -1);
}

BOOL CCustToolbar::IsWindowIconized(CWnd *pToolbar)
{
	int nIndex;

	nIndex = FindDragToolbarFromWindow(pToolbar, m_pToolbarArray);
	if(nIndex == -1)
	{
		nIndex = FindDragToolbarFromWindow(pToolbar, m_pHiddenToolbarArray);
		if(nIndex != -1)
			return !m_pHiddenToolbarArray[nIndex]->GetOpen();
	}
	else
		return !m_pToolbarArray[nIndex]->GetOpen();

	return TRUE;
	
}

int	 CCustToolbar::GetWindowPosition(CWnd *pToolbar)
{
	int nIndex;

	nIndex = FindDragToolbarFromWindow(pToolbar, m_pToolbarArray);
	if(nIndex == -1)
		nIndex = FindDragToolbarFromWindow(pToolbar, m_pHiddenToolbarArray);
	return nIndex;
}

void CCustToolbar::ShowDragToolbar(int nIndex, BOOL bShow)
{
	if(bShow)
	{
		if(nIndex != -1)
		{
			if(m_pHiddenToolbarArray[nIndex]->GetOpen())
				m_nNumOpen++;
			m_nNumShowing++;
			m_nActiveToolbars++;
			m_pToolbarArray[nIndex] = m_pHiddenToolbarArray[nIndex];
			m_pHiddenToolbarArray[nIndex] = NULL;
			m_pToolbarArray[nIndex]->GetToolbar()->ShowWindow(SW_SHOW);
			if(m_pToolbarArray[nIndex]->GetOpen()){
				CheckAnimationChangedToolbar(m_pToolbarArray[nIndex], nIndex, TRUE);
				m_pToolbarArray[nIndex]->ShowWindow(SW_SHOW);
			}
		}


	}
	else
	{
		if(nIndex != -1)
		{
			if(m_pToolbarArray[nIndex]->GetOpen())
				m_nNumOpen--;
			m_nNumShowing--;
			m_nActiveToolbars--;
			m_pHiddenToolbarArray[nIndex] = m_pToolbarArray[nIndex];
			m_pToolbarArray[nIndex] = NULL;
			m_pHiddenToolbarArray[nIndex]->ShowWindow(SW_HIDE);
			m_pHiddenToolbarArray[nIndex]->GetToolbar()->ShowWindow(SW_HIDE);
			CheckAnimationChangedToolbar(m_pHiddenToolbarArray[nIndex], nIndex, FALSE);

		}
	}
	m_pParent->RecalcLayout();
	RedrawWindow();
}

void CCustToolbar::ShowToolbar(CWnd *pToolbar, BOOL bShow)
{
	int nIndex = FindDragToolbarFromWindow(pToolbar, bShow ? m_pHiddenToolbarArray : m_pToolbarArray);

	if(nIndex != -1)
		ShowDragToolbar(nIndex, bShow);

}

void CCustToolbar::ShowToolbar(UINT nToolbarID, BOOL bShow)
{
	int nIndex = FindDragToolbarFromID(nToolbarID, bShow ? m_pHiddenToolbarArray : m_pToolbarArray);

	if(nIndex != -1)
		ShowDragToolbar(nIndex, bShow);
}

void CCustToolbar::RenameToolbar(UINT nOldID, UINT nNewID, UINT nNewToolTipID)
{
	int nIndex = FindDragToolbarFromID(nOldID, m_pToolbarArray);
	CDragToolbar *pToolbar = NULL;

	if(nIndex != -1)
		pToolbar = m_pToolbarArray[nIndex];
	else
	{
		nIndex = FindDragToolbarFromID(nOldID, m_pHiddenToolbarArray);
		if(nIndex != -1)
			pToolbar = m_pHiddenToolbarArray[nIndex];
	}

	if(pToolbar != NULL)
	{
		pToolbar->SetToolbarID(nNewID);
		CString tipStr;
		tipStr.LoadString(nNewToolTipID);
		pToolbar->SetTabTip(tipStr);

	}
}


CWnd *CCustToolbar::GetToolbar(UINT nToolbarID)
{
	int nIndex;

	nIndex = FindDragToolbarFromID(nToolbarID, m_pHiddenToolbarArray);
	
	if(nIndex == -1)
	{
		nIndex = FindDragToolbarFromID(nToolbarID, m_pToolbarArray);

		if(nIndex != -1)
			return m_pToolbarArray[nIndex]->GetToolbar();
	}
	else
		return m_pHiddenToolbarArray[nIndex]->GetToolbar();

	return NULL;
	
}
   
// Function	- CalcDynamicLayout
//
// Tells the framework how big this controlbar needs to be.  The height is based on the
// heights of the toolbars.  The width is set to 32767 which sets the width to the width
// of our parent
//
// Arguments - both are disregarded
//
// Returns - the dimensions we need to draw ourself
CSize CCustToolbar::CalcDynamicLayout(int nLength, DWORD dwMode )
{

	int height=0;

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL)
		{
			 if(m_pToolbarArray[i]->GetOpen())
			 {
				 height += m_pToolbarArray[i]->GetToolbarHeight() + SPACE_BETWEEN_TOOLBARS; 
			 }
		}
	}

//	height += (m_nNumOpen ) * SPACE_BETWEEN_TOOLBARS;

	if(m_nNumOpen != m_nActiveToolbars && m_nNumShowing != 0 || m_externalTabArray.GetSize() > 0)
		height += CLOSED_BUTTON_HEIGHT + SPACE_BETWEEN_TOOLBARS;

	//because all closed tabs may have changed position, read the tools
	if(height >= CLOSED_BUTTON_HEIGHT)
		ChangeToolTips(height);

	if(m_bBottomBorder)
		// if we have a bottom border then add space for separator
		height += SPACE_BETWEEN_TOOLBARS;

	return CSize(32767, height);

}

void CCustToolbar::SetToolbarStyle(int nToolbarStyle)
{

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL)
		{
			m_pToolbarArray[i]->SetToolbarStyle( nToolbarStyle );
		}
	}
    
    if( m_nAnimationPos != -1 )
    {
        // Reset the animation (height change)
        m_pToolbarArray[m_nAnimationPos]->SetAnimation( m_pAnimation );
    }
    
	m_pParent->RecalcLayout();
	RedrawWindow();


}

void CCustToolbar::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{
	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL)
		{
			m_pToolbarArray[i]->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
		}
	}
}

void CCustToolbar::UpdateURLBars(char* url)
{
	for (int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL)
		{
			m_pToolbarArray[i]->UpdateURLBars(url);
		}
	}
}

void CCustToolbar::Customize(CRDFToolbar *pLinkToolbar, int nSelectedButton)
{
}

BOOL CCustToolbar::GetSaveToolbarInfo(void)
{
	return m_bSaveToolbarInfo;
}

void CCustToolbar::SetSaveToolbarInfo(BOOL bSaveToolbarInfo)
{

	m_bSaveToolbarInfo = bSaveToolbarInfo;
}

void CCustToolbar::SetNewParentFrame(CFrameWnd *pParent)
{
	if(m_pParent == pParent)
		return;

#ifdef XP_WIN32
	if(m_pParent)
		m_pParent->RemoveControlBar(this);
#endif

	SetParent(pParent);

#ifdef XP_WIN32
	if(pParent)
		pParent->AddControlBar(this);
#endif

	m_pParent = pParent;

}

void CCustToolbar::AddExternalTab(CWnd *pOwner, HTAB_BITMAP eHTabType, UINT nTipID, UINT nTabID)
{
	CCustToolbarExternalTab *pExtTab = new CCustToolbarExternalTab(pOwner, eHTabType, nTipID, nTabID);

	m_externalTabArray.Add(pExtTab);

	RedrawWindow();
}

void CCustToolbar::RemoveExternalTab(UINT nTabID)
{

	int nSize = m_externalTabArray.GetSize();

	for(int i = 0; i < nSize; i++)
	{
		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab *)m_externalTabArray[i];

		if(pExtTab->GetTabID() == nTabID){
			m_externalTabArray.RemoveAt(i);
			delete pExtTab;
			RedrawWindow();
			return;
		}
	}

}

void CCustToolbar::SetBottomBorder(BOOL bBottomBorder)
{
	m_bBottomBorder = bBottomBorder;

}


//////////////////////////////////////////////////////////////////////////
//					Messages for CCustToolbar
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CCustToolbar, CControlBar)
	//{{AFX_MSG_MAP(CControlBar)
    ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(CT_HIDETOOLBAR, OnHideToolbar)
	ON_MESSAGE(CT_DRAGTOOLBAR, OnDragToolbar)
	ON_MESSAGE(CT_DRAGTOOLBAR_OVER, OnDragToolbarOver)
	ON_MESSAGE(CT_CUSTOMIZE, OnCustomize)
#ifndef WIN32
	ON_MESSAGE(WM_SIZEPARENT, OnSizeParent)
#endif
	ON_WM_TIMER()
	ON_WM_PALETTECHANGED()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CCustToolbar::OnSize( UINT nType, int cx, int cy )
{
	CDragToolbar *pToolbar;
	int nToolbarHeight;

	int yStart=(m_nNumOpen == 0) ? 0 : SPACE_BETWEEN_TOOLBARS;

	BOOL bWindowsShown = FALSE;

	int nWidth;

	for(int i=0; i<m_nNumToolbars; i++)
	{
		nWidth = cx;

		if(m_pToolbarArray[i] != NULL)
		{
			pToolbar = m_pToolbarArray[i];
			nToolbarHeight = pToolbar->GetToolbarHeight();
			
			if(pToolbar->GetOpen())
			{

				pToolbar->MoveWindow(0, yStart, nWidth, nToolbarHeight);

				yStart += nToolbarHeight + SPACE_BETWEEN_TOOLBARS;
			}
		}
	}
}

void CCustToolbar::OnPaint()
{
	CRect rcClient, updateRect, toolbarRect, intersectRect;

	GetClientRect(&rcClient);
	GetUpdateRect(&updateRect);

	CPaintDC dcPaint(this);	// device context for painting

	HDC hSrcDC = dcPaint.m_hDC;

	HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());

	HDC hMemDC = ::CreateCompatibleDC(hSrcDC);
	HBITMAP hbmMem = CreateCompatibleBitmap(hSrcDC,
                                    rcClient.Width(),
                                    rcClient.Height());

	COLORREF rgbOldBk = ::SetBkColor(hMemDC, sysInfo.m_clrBtnFace);

    //
    // Select the bitmap into the off-screen DC.
    //

	HBITMAP hbmOld;
    hbmOld = (HBITMAP)::SelectObject(hMemDC, hbmMem);

	// Use our background color
		// Use the button face color as our background
	HBRUSH brFace = ::CreateSolidBrush(RGB(128, 128, 128));

	int nExternalTabArrSize = m_externalTabArray.GetSize();

	CRect clientCopy = rcClient;

	if(m_nNumOpen != m_nActiveToolbars || nExternalTabArrSize > 0)
	{
		clientCopy.bottom -= CLOSED_BUTTON_HEIGHT;
	}
	::FillRect(hMemDC, clientCopy, brFace);
	::DeleteObject(brFace);

	if(m_nNumOpen != m_nActiveToolbars || nExternalTabArrSize > 0)
	{
		clientCopy.top = clientCopy.bottom;
		clientCopy.bottom += CLOSED_BUTTON_HEIGHT;
		::FillRect(hMemDC, clientCopy, sysInfo.m_hbrBtnFace);
	}

	int nClosedStartX = 0;
	int nStartY = 0;

	if(m_nNumOpen != 0)
	{
		DrawSeparator(hMemDC, rcClient.left, rcClient.right, nStartY, TRUE);
	}
	else
	{
		DrawSeparator(hMemDC, rcClient.left, rcClient.right, nStartY, FALSE);
	}
	nStartY += SPACE_BETWEEN_TOOLBARS;

	//These are the number of each button we've seen so far as compared to m_nNumOpen which is
	//the total number of toolbars we know are showing
	int nNumClosedButtons = 0;
	int nNumOpenButtons = 0;

	int nBottom = m_bBottomBorder ? rcClient.bottom - SPACE_BETWEEN_TOOLBARS : rcClient.bottom;

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL) 
		{
			if(m_pToolbarArray[i]->GetOpen())
			{
				int nToolbarHeight = m_pToolbarArray[i]->GetToolbarHeight();

				nStartY += nToolbarHeight;
				nNumOpenButtons++;

				if(nNumOpenButtons != m_nNumOpen)
				{
					DrawSeparator(hMemDC, rcClient.left, rcClient.right, nStartY, TRUE);
				}
				else
				{
					DrawSeparator(hMemDC, rcClient.left, rcClient.right, nStartY, FALSE);
				}

				nStartY +=SPACE_BETWEEN_TOOLBARS;
			}
			else
			{
				HTAB_BITMAP tabType = m_pToolbarArray[i]->GetHTabType();

				DrawClosedTab(hSrcDC, hMemDC, tabType, nNumClosedButtons, i == m_nMouseOverTab,
							  nClosedStartX, nBottom);


				nClosedStartX += GetNextClosedButtonX(tabType, nNumClosedButtons, nClosedStartX);
				nNumClosedButtons++;
				
			}
		}
	}


	for(int j = 0; j < nExternalTabArrSize; j++)
	{
		HTAB_BITMAP tabType;

		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab *)m_externalTabArray[j];

		tabType = pExtTab->GetHTabType();

		DrawClosedTab(hSrcDC, hMemDC, tabType, nNumClosedButtons, j + m_nNumToolbars == m_nMouseOverTab,
					  nClosedStartX, nBottom);

		nClosedStartX += GetNextClosedButtonX(tabType, nNumClosedButtons, nClosedStartX);
		nNumClosedButtons++;
	}

	if(m_bBottomBorder)
		DrawSeparator(hMemDC, rcClient.left, rcClient.right, rcClient.bottom - 2, TRUE);
	
	::BitBlt(hSrcDC, 0, 0, rcClient.Width(), rcClient.Height(), hMemDC, 0, 0,
					SRCCOPY);

 	::SetBkColor(hMemDC, rgbOldBk);

    ::SelectObject(hMemDC, hbmOld);
    ::DeleteObject(hbmMem);
 
	::DeleteDC(hMemDC);
}

void CCustToolbar::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CCustToolbar::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}

void CCustToolbar::OnLButtonUp(UINT nFlags, CPoint point)
{
	
	int nResult;

	MSG msg = *(GetCurrentMessage());
	m_toolTip.RelayEvent(&msg);

	if((nResult = CheckClosedButtons(point)) != -1)
	{
		if(nResult < m_nNumToolbars)
			OpenDragToolbar(nResult);
		// then we must have clicked on one of the external tabs
		else
			OpenExternalTab(nResult - m_nNumToolbars);

	}
}

void CCustToolbar::OnLButtonDown(UINT nFlags, CPoint point)
{

	MSG msg = *(GetCurrentMessage());
	m_toolTip.RelayEvent(&msg);

	CWnd::OnLButtonDown(nFlags, point);
}

void CCustToolbar::OnMouseMove(UINT nFlags, CPoint point)
{
	m_toolTip.Activate(TRUE);
	MSG msg = *(GetCurrentMessage());
	m_toolTip.RelayEvent(&msg);

	CRect clientRect;

	GetClientRect(&clientRect);
	if(m_nNumOpen != m_nNumToolbars)
	{
		clientRect.top = clientRect.bottom - CLOSED_BUTTON_HEIGHT;
		if(PtInRect(&clientRect, point))
		{
			int nMouseOverTab = CheckClosedButtons(point);

			if(m_nMouseOverTab != nMouseOverTab)
			{
				m_nMouseOverTab= nMouseOverTab;
				if(m_nTabHaveFocusTimer == 0)
					m_nTabHaveFocusTimer = SetTimer(IDT_TABHAVEFOCUS, TABHAVEFOCUS_DELAY, NULL);
				InvalidateRect(clientRect);
			}
		}
	}
	CWnd::OnMouseMove(nFlags, point);
}

LRESULT CCustToolbar::OnHideToolbar(WPARAM wParam, LPARAM lParam)
{

	HWND hwnd = (HWND)lParam;
	CDragToolbar *pToolbar = (CDragToolbar*)CWnd::FromHandlePermanent(hwnd);

	pToolbar->SetOpen(FALSE);
	pToolbar->ShowWindow(SW_HIDE);
	m_nNumOpen--;

	int nIndex = FindIndex(pToolbar);
	CheckAnimationChangedToolbar(pToolbar, nIndex, FALSE);

	m_pParent->RecalcLayout();
	RedrawWindow();

	return 1;
}

LRESULT CCustToolbar::OnDragToolbar(WPARAM wParam, LPARAM lParam)
{

	HWND hwnd = (HWND)wParam;
	CPoint point;

	point.x = LOWORD(lParam);
	point.y = HIWORD(lParam);


	CDragToolbar *pToolbar = (CDragToolbar*)CWnd::FromHandlePermanent(hwnd);

	CRect rect;
	pToolbar->GetWindowRect(&rect);

	//if the point is not in our rect, we don't want to move the window
	CRect clientRect;
	GetClientRect(&clientRect);
	if(m_nNumOpen != m_nActiveToolbars)
	{
		clientRect.bottom -=  CLOSED_BUTTON_HEIGHT;
	}
	//take into account the height plus the 2 pixels we added on earlier
	clientRect.bottom -= rect.Height() - DRAGGING_BORDER_HEIGHT;

	if(!clientRect.PtInRect(point))
		return 1;

    pToolbar->SetWindowPos(NULL, 0, point.y, rect.Width(), rect.Height(), SWP_NOSIZE );
	ScreenToClient(&rect);

	if(m_oldDragPoint.x != -1 && m_oldDragPoint.y != -1)
	{
		int nDir;

		nDir = m_oldDragPoint.y - point.y;

		//dragging down
		if(nDir < 0)
		{
			CWnd *pChild = FindToolbarFromPoint(CPoint(0, point.y + rect.Height() - 5), pToolbar);
			if(pChild != NULL && pChild != pToolbar && pChild != this)
			{
				SwitchChildren((CDragToolbar*)pToolbar, (CDragToolbar*)pChild, -1, point.y);

			}
		}//dragging up
		else if(nDir > 0)
		{
			CWnd *pChild = FindToolbarFromPoint(CPoint(0, point.y + 5), pToolbar);
			if(pChild != NULL && pChild != pToolbar && pChild != this)
			{
				SwitchChildren((CDragToolbar*)pToolbar, (CDragToolbar*)pChild, 1, point.y);

			}
		}

	}

	m_oldDragPoint = point;

	return 1;
}

LRESULT CCustToolbar::OnDragToolbarOver(WPARAM wParam, LPARAM lParam)
{

	HWND hwnd = (HWND)lParam;
	CDragToolbar *pToolbar = (CDragToolbar*)CWnd::FromHandlePermanent(hwnd);

	int nIndex = FindIndex(pToolbar);

	if(nIndex != -1)
	{
		int nYStart = (m_nNumOpen == 0) ? 0 : SPACE_BETWEEN_TOOLBARS;
		for(int i = 0; i < nIndex; i++)
		{
			if(m_pToolbarArray[i] != NULL && m_pToolbarArray[i]->GetOpen())
			{
				nYStart += m_pToolbarArray[i]->GetToolbarHeight() + SPACE_BETWEEN_TOOLBARS;
			}

		}
		pToolbar->SetWindowPos(NULL, 0, nYStart, 0, 0, SWP_NOSIZE); 

	}

	GetParentFrame()->RecalcLayout();

	return 1;
}

LRESULT CCustToolbar::OnCustomize(WPARAM wParam, LPARAM lParam)
{

	CRDFToolbar *pToolbar = (CRDFToolbar *)CWnd::FromHandlePermanent((HWND)lParam);
	int nButton = LOWORD(wParam);

	Customize(pToolbar, wParam);
	return 1;
}

void CCustToolbar::OnTimer( UINT  nIDEvent )
{

	CRect rect;
	GetWindowRect(&rect);
	POINT pt;
	GetCursorPos(&pt);

 

	if(nIDEvent == IDT_TABHAVEFOCUS)
	{
		rect.top = rect.bottom - CLOSED_BUTTON_HEIGHT;
		BOOL bMouseInTabs = PtInRect(&rect, pt);		
		if(!bMouseInTabs)
		{
			ScreenToClient(&rect);
			KillTimer(m_nTabHaveFocusTimer);
			m_nTabHaveFocusTimer = 0;
			m_nMouseOverTab = -1;
			InvalidateRect(&rect, FALSE);
		}
	}
	CWnd::OnTimer(nIDEvent);

}
#ifndef WIN32
LRESULT CCustToolbar::OnSizeParent(WPARAM wParam, LPARAM lParam)
{
	m_sizeFixedLayout = CalcDynamicLayout(0, 0);
	return CControlBar::OnSizeParent(wParam, lParam);
}
#endif


void CCustToolbar::OnPaletteChanged( CWnd* pFocusWnd )
{
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
//					CCustToolbar Helper Functions
//////////////////////////////////////////////////////////////////////////

int CCustToolbar::CheckOpenButtons(CPoint point)
{

	if(point.x < 0 || point.x > OPEN_BUTTON_WIDTH)
	{
		return -1;
	}

	int nStartY = 0;
	int nToolbarHeight;

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL && m_pToolbarArray[i]->GetOpen())
		{
			nToolbarHeight = m_pToolbarArray[i]->GetToolbarHeight();

			if(point.y >= nStartY && point.y <= nStartY + nToolbarHeight)
			{
				return i;
			}

			nStartY += nToolbarHeight + SPACE_BETWEEN_TOOLBARS;
		}
	}

	return -1;

}

int CCustToolbar::CheckClosedButtons(CPoint point)
{

	CRect rcClient;

	GetClientRect(&rcClient);

	int nStartX = 0;
	int nNumClosedButtons = 0;

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL && !m_pToolbarArray[i]->GetOpen())
		{
			HTAB_BITMAP tabType = m_pToolbarArray[i]->GetHTabType();
			if(PointInClosedTab(point, tabType, nNumClosedButtons, nStartX, rcClient.bottom))
				return i;

			nStartX += GetNextClosedButtonX(tabType, nNumClosedButtons, nStartX);
			nNumClosedButtons++;
		}
	}

	int nExtTabSize = m_externalTabArray.GetSize();

	for(int j = 0; j < nExtTabSize; j++)
	{
		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab *)m_externalTabArray[j];

		HTAB_BITMAP tabType = pExtTab->GetHTabType();
		if(PointInClosedTab(point, tabType, nNumClosedButtons, nStartX, rcClient.bottom))
			return j + m_nNumToolbars;


		nStartX += GetNextClosedButtonX(tabType, nNumClosedButtons, nStartX);


		nNumClosedButtons++;
	}

	return -1;

}

BOOL CCustToolbar::PointInClosedTab(CPoint point, HTAB_BITMAP tabType, int nNumClosedButtons, int nStartX, int nBottom)
{
	CRgn rgn;

	GetClosedButtonRegion(tabType, nNumClosedButtons, rgn);
	rgn.OffsetRgn(nStartX, nBottom - CLOSED_BUTTON_HEIGHT);
	if(rgn.PtInRegion(point))
	{
		rgn.DeleteObject();
		return TRUE;
	}
	rgn.DeleteObject();
	return FALSE;
}

// if bToolbarSeparator is true then draw a grey line and a white line.  If it's false then it is the
// separator between the last toolbar and the closed buttons so draw a grey line and a black line
void CCustToolbar::DrawSeparator(HDC hDC, int nStartX, int nEndX, int nStartY, BOOL bToolbarSeparator)
{

	HPEN pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
	HPEN pOldPen = (HPEN)::SelectObject(hDC, pen);

#if defined (WIN32)
	::MoveToEx(hDC, nStartX, nStartY, NULL);
#else
	::MoveTo(hDC, nStartX, nStartY);
#endif

	::LineTo(hDC, nEndX, nStartY);

	::SelectObject(hDC, pOldPen);

	::DeleteObject(pen);

	if(bToolbarSeparator)
	{       
	#if defined(WIN32)
		pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHILIGHT));
	#else
		pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT)); 
	#endif
	
	}
	else
	{
		pen = ::CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	}

	::SelectObject(hDC, pen);

#if defined (WIN32)
	::MoveToEx(hDC, nStartX, nStartY + 1, NULL);
#else
	::MoveTo(hDC, nStartX, nStartY + 1);
#endif

	::LineTo(hDC, nEndX, nStartY + 1);

	::SelectObject(hDC, pOldPen);
	::DeleteObject(pen);


}

//if dir < 0 then move pSwitch above pOriginal.  If dir > 0 then move pSwitch below pOriginal
void CCustToolbar::SwitchChildren(CDragToolbar *pOriginal, CDragToolbar *pSwitch, int dir, 
								  int topYPoint)
{
	int nOriginalPos = FindIndex(pOriginal);
	int nSwitchPos = FindIndex (pSwitch);

	if(nOriginalPos == -1 || nSwitchPos == -1)
		return;

	//if we've already switched these two (i.e. moving down and pOriginal already below pSwitch
	//then don't continue
	if((dir <0 && nOriginalPos > nSwitchPos) || (dir > 0 && nSwitchPos > nOriginalPos))
		return;

	m_pToolbarArray[nOriginalPos] = pSwitch;
	m_pToolbarArray[nSwitchPos] = pOriginal;

	if((nSwitchPos == m_nAnimationPos && dir > 0) ||
	   (nOriginalPos == m_nAnimationPos && dir < 0))
	{
		// The animation is hopping from one toolbar to another.
		// These two toolbars could change their heights following
		// the movement of the animation.  In this case, we care
		// about the one being dragged.  It had a silly drag border
		// appended to its height... we need to keep that around.
		// pOriginal is the one being dragged.
		int height = pOriginal->GetToolbarHeight();

		if(dir < 0)
		{
			// Moving down.  Animation leaves the bar being dragged
			// and hops onto the new one.  The bar being dragged
			// could shrink in size.
			pOriginal->SetAnimation(NULL);			// Order of these is relevant.
			pSwitch->SetAnimation(m_pAnimation);	// Dont want to have anim. on two simult.
		}
		else 
		{
			// Moving up.  Animation pops onto the bar being dragged
			// and leaves the old one.  The bar being dragged could
			// expand.
			pSwitch->SetAnimation(NULL);			// Order of these is relevant.
			pOriginal->SetAnimation(m_pAnimation);	// Dont want to have anim. on two simult.
		}
		
		CRect clientRect;
		pOriginal->GetClientRect(&clientRect);
		MapWindowPoints(this, clientRect);

		// We recalced our layout to accommodate the animation.
		// Append the dragging_border_height back in if the height changed.
		int newHeight = pOriginal->GetToolbarHeight();

		// Now the window has to be moved to the correct yPoint location.  
				
		if ((newHeight < height) && (pOriginal->GetMouseOffsetWithinToolbar() > newHeight))
		{
			// If the window shrank, then the user's drag pointer may become
			// invalid. Therefore for this case only, we have to move the toolbar
			// so that it is still underneath the user's drag pointer.

			// Preserve the yoffset from the bottom of the drag. (Hey, gotta
			// do something...)
			// Toolbar moves down by the difference in the two heights.
			int diff = height - newHeight;
			if (diff > newHeight)	// Handle the (rare and highly improbable)
			{						// case where the toolbar more than halves its height
				
				topYPoint += pOriginal->GetMouseOffsetWithinToolbar() - newHeight/2;
				// In this (preposterously rare) case, snap the toolbar so that it is
				// sitting with its halfway point (vertically) under the mouse pointer.
				// Adjust our cached mouse offset
				pOriginal->SetMouseOffsetWithinToolbar(newHeight/2);
			}
			else 
			{
				topYPoint += diff;

				// Adjust our cached mouse offset
				pOriginal->SetMouseOffsetWithinToolbar(pOriginal->GetMouseOffsetWithinToolbar() - 
														diff);
			}
		}
		
		// Set the new position and size
		pOriginal->SetWindowPos(NULL, clientRect.left, topYPoint, clientRect.Width(), newHeight + DRAGGING_BORDER_HEIGHT, SWP_NOZORDER);	
	}

	int nYStart = (m_nNumOpen == 0) ? 0 : SPACE_BETWEEN_TOOLBARS;
	for(int i = 0; i < nOriginalPos; i++)
	{
		if(m_pToolbarArray[i] != NULL && m_pToolbarArray[i]->GetOpen())
		{
			nYStart += m_pToolbarArray[i]->GetToolbarHeight() + SPACE_BETWEEN_TOOLBARS;
		}
	}

	pSwitch->SetWindowPos(&wndBottom, 0, nYStart, 0, 0, SWP_NOSIZE); 
}

//Finds the child window that is at the given point that is not pIgnore.  If this
//child doesn't exist, NULL is returned.
CDragToolbar *CCustToolbar::FindToolbarFromPoint(CPoint point, CDragToolbar *pIgnore)
{
	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL && m_pToolbarArray[i] != pIgnore && m_pToolbarArray[i]->GetOpen())
		{
			CRect clientRect;

			MapWindowPoints(m_pToolbarArray[i], &point, 1);
			m_pToolbarArray[i]->GetClientRect(&clientRect);

			if(clientRect.PtInRect(point))
				return m_pToolbarArray[i];
		}
	}

	return NULL;
}

int CCustToolbar::FindIndex(CDragToolbar *pToolbar)
{

	for(int i = 0; i < m_nNumToolbars; i++)
	{

		if(m_pToolbarArray[i] == pToolbar)
			return i;

	}
	
	return -1;
}

//Finds the first showing toolbar after the given index
int CCustToolbar::FindFirstShowingToolbar(int nIndex)
{
	for(int i = nIndex + 1; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL && m_pToolbarArray[i]->GetOpen())
			return i;
	}

	return -1;

}

HBITMAP CCustToolbar::GetClosedButtonBitmap(HTAB_BITMAP tabType, int nNumClosedButtons)
{
	switch(tabType)
	{
		case eLARGE_HTAB:
			if(nNumClosedButtons == 0)
				return m_pHorizTabArray[LARGE_FIRST];
			else
				return m_pHorizTabArray[LARGE_OTHER];;
			break;
		case eSMALL_HTAB:
			if(nNumClosedButtons == 0)
				return m_pHorizTabArray[SMALL_FIRST];
			else
				return m_pHorizTabArray[SMALL_OTHER];
	
	}

	return NULL;
}

int CCustToolbar::GetNextClosedButtonX(HTAB_BITMAP tabType, int nNumClosedButtons, int nClosedStartX)
{

	switch(tabType)
	{
		case eLARGE_HTAB:
			if(nNumClosedButtons == 0)
				return 53;
			else
				return 56;
			break;
		case eSMALL_HTAB:
			if(nNumClosedButtons == 0)
				return 29;
			else
				return 33;
			break;
	}

	return 0;

}

void  CCustToolbar::GetClosedButtonRegion(HTAB_BITMAP tabType, int nNumClosedButtons, CRgn &rgn)
{

	POINT ptArray[4];

	switch(tabType)
	{
		case eLARGE_HTAB:
			if(nNumClosedButtons == 0)
			{
				ptArray[0].x = 0;
				ptArray[0].y = 0;
				ptArray[1].x = 62;
				ptArray[1].y = 0;
				ptArray[2].x = 53;
				ptArray[2].y = 9;
				ptArray[3].x = 0;
				ptArray[3].y = 9;
			}
			else
			{
				ptArray[0].x = 9;
				ptArray[0].y = 0;
				ptArray[1].x = 65;
				ptArray[1].y = 0;
				ptArray[2].x = 56;
				ptArray[2].y = 9;
				ptArray[3].x = 0;
				ptArray[3].y = 9;
			}
			break;
		case eSMALL_HTAB:
			if(nNumClosedButtons == 0)
			{
				ptArray[0].x = 0;
				ptArray[0].y = 0;
				ptArray[1].x = 38;
				ptArray[1].y = 0;
				ptArray[2].x = 29;
				ptArray[2].y = 9;
				ptArray[3].x = 0;
				ptArray[3].y = 9;
			}
			else
			{
				ptArray[0].x = 9;
				ptArray[0].y = 0;
				ptArray[1].x = 42;
				ptArray[1].y = 0;
				ptArray[2].x = 33;
				ptArray[2].y = 9;
				ptArray[3].x = 0;
				ptArray[3].y = 9;
			}
			break;
	}
	rgn.CreatePolygonRgn(ptArray, 4, WINDING);
}

HBITMAP CCustToolbar::CreateHorizTab(UINT nID)
{

	HINSTANCE hInst = AfxGetResourceHandle();
	HDC hDC = ::GetDC(m_hWnd);

	HPALETTE hOldPalette = ::SelectPalette(hDC, WFE_GetUIPalette(GetParentFrame()), FALSE);

	HBITMAP bitmap = wfe_LoadBitmap(hInst, hDC, MAKEINTRESOURCE(nID));

	::SelectPalette(hDC, hOldPalette, TRUE);
	::ReleaseDC(m_hWnd, hDC);

	return bitmap;
}

int CCustToolbar::FindDragToolbarFromWindow(CWnd *pWindow, CDragToolbar **pToolbarArray)
{

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(pToolbarArray[i] != NULL)
		{
			if(pToolbarArray[i]->GetToolbar() == pWindow)
				return i;
		}
	}

	return -1;


}

int CCustToolbar::FindDragToolbarFromID(UINT nToolbarID, CDragToolbar **pToolbarArray)
{

	for(int i = 0; i < m_nNumToolbars; i++)
	{
		if(pToolbarArray[i] != NULL)
		{
			if(pToolbarArray[i]->GetToolbarID() == nToolbarID)
				return i;
		}
	}

	return -1;


}

void CCustToolbar::OpenDragToolbar(int nIndex)
{
	m_pToolbarArray[nIndex]->SetOpen(TRUE);
	m_pToolbarArray[nIndex]->ShowWindow(SW_SHOW);

	CheckAnimationChangedToolbar(m_pToolbarArray[nIndex], nIndex, TRUE);

	m_nNumOpen++;
	m_pParent->RecalcLayout();
	RedrawWindow();
}

void CCustToolbar::OpenExternalTab(int nIndex)
{
	if( nIndex >= 0 && nIndex < m_externalTabArray.GetSize())
	{
		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab*)m_externalTabArray[nIndex];
		pExtTab->GetOwner()->SendMessage(WM_COMMAND, IDC_COLLAPSE);
		m_externalTabArray.RemoveAt(nIndex);
		delete pExtTab;
		m_pParent->RecalcLayout();
		RedrawWindow();
	}
}

// Checks to see if the animation has to go on a different toolbar.  If bOpen is TRUE,
// this checks to see if opening toolbar gets the animation.  If bOpen is FALSE, and
// pToolbar has the animation, it removes it and finds the new toolbar to give it to
void CCustToolbar::CheckAnimationChangedToolbar(CDragToolbar *pToolbar, int nIndex, BOOL bOpen)
{

	if(bOpen)
	{
		if(nIndex < m_nAnimationPos || m_nAnimationPos == -1)
		{
			if(m_nAnimationPos != -1)
			{
				m_pToolbarArray[m_nAnimationPos]->SetAnimation(NULL);
			}
			else
			{
				if(m_pAnimation)
					m_pAnimation->ShowWindow(SW_SHOW);
			}
			pToolbar->SetAnimation(m_pAnimation);
			m_nAnimationPos = nIndex;
		}
	}
	else
	{
		if(nIndex == m_nAnimationPos)
		{
			//This sets m_nAnimationPos to -1 if everything is closed
			int nFirstShowing = FindFirstShowingToolbar(nIndex);
			m_nAnimationPos = nFirstShowing;

			if(m_nAnimationPos != -1)
			{
				m_pToolbarArray[m_nAnimationPos]->SetAnimation(m_pAnimation);
			}
			else
			{
				if(m_pAnimation)
					m_pAnimation->ShowWindow(SW_HIDE);
			}

			pToolbar->SetAnimation(NULL);
		}

	}

}

void CCustToolbar::ChangeToolTips(int nHeight)
{
	CDragToolbar *pToolbar = NULL;
	int i;

	// First remove all tools
	for(i = 0; i < m_nNumToolbars; i++)
	{
		if(m_pToolbarArray[i] != NULL)
			pToolbar = (CDragToolbar *) m_pToolbarArray[i];
		else if(m_pHiddenToolbarArray[i] != NULL)
			pToolbar = (CDragToolbar *) m_pHiddenToolbarArray[i];

		if(pToolbar != NULL && pToolbar->GetToolID() != NOTOOL)
		{
#ifdef XP_WIN32
			m_toolTip.DelTool(this, pToolbar->GetToolID());
#else
			m_toolTip.DelTool(m_hWnd, pToolbar->GetToolID());
#endif

			pToolbar->SetToolID(NOTOOL);
		}
	}

	int nExtTabSize = m_externalTabArray.GetSize();

	for(i = 0 ; i < nExtTabSize; i ++)
	{
		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab *)m_externalTabArray[i];

#ifdef XP_WIN32
		m_toolTip.DelTool(this, pExtTab->GetToolID());
#else
		m_toolTip.DelTool(m_hWnd, pExtTab->GetToolID());
#endif

		pExtTab->SetToolID(NOTOOL);
	}

	// Now Set the Tools for those toolbars that are visible and closed
	int nNumClosedButtons = 0;
	int nClosedStartX = 0;
	CRect toolRect;

	for(int j = 0; j < m_nNumToolbars; j++)
	{
		pToolbar = (CDragToolbar *) m_pToolbarArray[j];

		if(pToolbar && !pToolbar->GetOpen())
		{
			HTAB_BITMAP tabType = pToolbar->GetHTabType();
			FindToolRect(toolRect, tabType, nClosedStartX, nHeight - CLOSED_BUTTON_HEIGHT, nNumClosedButtons); 
			nClosedStartX += GetNextClosedButtonX(tabType, nNumClosedButtons, nClosedStartX);
			nNumClosedButtons++;
			m_toolTip.AddTool(this, pToolbar->GetTabTip(), &toolRect, nNumClosedButtons);
			pToolbar->SetToolID(nNumClosedButtons);
		}
	}


	for(int k = 0; k < nExtTabSize; k++)
	{
		CCustToolbarExternalTab *pExtTab = (CCustToolbarExternalTab *)m_externalTabArray[k];

		HTAB_BITMAP tabType = pExtTab->GetHTabType();
		FindToolRect(toolRect, tabType, nClosedStartX, nHeight - CLOSED_BUTTON_HEIGHT, nNumClosedButtons); 
		nClosedStartX += GetNextClosedButtonX(tabType, nNumClosedButtons, nClosedStartX);
		nNumClosedButtons++;
		m_toolTip.AddTool(this, pExtTab->GetTabTip(), &toolRect, nNumClosedButtons);
		pExtTab->SetToolID(nNumClosedButtons);

	}

}

void CCustToolbar::FindToolRect(CRect &toolRect, HTAB_BITMAP eTabType, int nStartX, int nStartY, int nButtonNum)
{
	switch(eTabType)
	{
		case eLARGE_HTAB:
			if(nButtonNum == 0)
			{
				toolRect.SetRect(nStartX, nStartY, nStartX + 53, nStartY + 9);
			}
			else
			{
				toolRect.SetRect(nStartX + 9, nStartY, nStartX + 56, nStartY + 9);
			}
			break;
		case eSMALL_HTAB:
			if(nButtonNum == 0)
			{
				toolRect.SetRect(nStartX, nStartY, nStartX + 29, nStartY + 9);
			}
			else
			{
				toolRect.SetRect(nStartX + 9, nStartY, nStartX + 33, nStartY +  9);
			}
			break;
	}


}

int  CCustToolbar::FindFirstAvailablePosition(void)
{

	for(int i = 0; i < m_nNumToolbars; i++)
	{

		if(m_pHiddenToolbarArray[i] == NULL && m_pToolbarArray[i] == NULL)
			return i;
	}

	return -1;

}

void CCustToolbar::DrawClosedTab(HDC hCompatibleDC, HDC hDestDC, HTAB_BITMAP tabType, int nNumClosedButtons,
								 BOOL bMouseOver, int nStartX, int nBottom)
{

	HBITMAP hBitmap = GetClosedButtonBitmap(tabType, nNumClosedButtons);

	if(hBitmap != NULL)
	{

		// Create a scratch DC and select our bitmap into it.
		HDC pBmpDC  = ::CreateCompatibleDC(hCompatibleDC);

		HBITMAP hOldBmp = (HBITMAP)::SelectObject(pBmpDC , hBitmap);
		// Get the image dimensions
		CSize sizeImg;
		BITMAP bmp;

		::GetObject(hBitmap, sizeof(bmp), &bmp);
		sizeImg.cx = bmp.bmWidth;
		sizeImg.cy = CLOSED_BUTTON_HEIGHT;
		
		CPoint bitmapStart(0, !bMouseOver ? 0 : CLOSED_BUTTON_HEIGHT);

		::FEU_TransBlt( hDestDC, nStartX, nBottom - sizeImg.cy, sizeImg.cx, sizeImg.cy,
						pBmpDC, bitmapStart.x, bitmapStart.y, WFE_GetUIPalette(GetParentFrame()), PALETTERGB(255, 0, 255) );

		// Cleanup
		::SelectObject(pBmpDC, hOldBmp);
		DeleteDC(pBmpDC);

	
	}



}
