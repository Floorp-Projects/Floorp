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
#include "mainfrm.h"
#include "urlbar.h"
#include "cxdc.h"
#include "custom.h"
#include "dropmenu.h"
#include "hk_funcs.h"
#include "prefapi.h"
#include "glhist.h"
#include "navfram.h"

extern void wfe_Progress(MWContext *pContext, const char *pMessage);

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


/****************************************************************************
*
*	CONSTANTS
*
****************************************************************************/

// Positions/dimensions for certain widgets (page rep icon, quick file button)

static const int DRAG_ICON_WIDTH = 16;
static const int DRAG_ICON_HEIGHT = 15;

static const int BOOKMARKS_TEXT_LEFT = 10;

BEGIN_MESSAGE_MAP(CURLBar, CURLBarBase) 
    //{{AFX_MSG_MAP(CURLBar)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDBLCLK()
    ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
    ON_WM_LBUTTONDOWN()
    ON_WM_SIZE()
    ON_WM_PAINT()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_WM_LBUTTONUP()
	ON_WM_CTLCOLOR()
	ON_WM_PALETTECHANGED()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CURLBar, CURLBarBase)
#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

// Buffers to hold all of the bitmaps.  Freed when the last URL bar
//   gets destructed
//
static int numberOfUrlBars = 0;
static CBitmap * pDragURLBitmap = NULL;

/////////////////////////////////////////////////////////////////////////////
// CURLBar dialog

CURLBar::CURLBar(): CURLBarBase(),
    m_cpLBDown(0,0),
	m_DragIconY(0)
{
    m_pBox = NULL;
    m_pIMWContext = NULL;
    m_nTextStatus = TRUE; // == "location" is displayed
    m_bAddToDropList = FALSE;

	m_pFont = NULL;
	m_DragIconX = 0;
	m_pPageProxy = NULL;
	m_hBackgroundBrush = CreateSolidBrush(RGB(192, 192, 192));
	numberOfUrlBars++;
	m_nBoxHeight = 0;
}

void CURLBar::SetContext( LPUNKNOWN pUnk )
{
	if (m_pIMWContext) {
		m_pIMWContext->Release();
		m_pIMWContext = NULL;
	}
	if (pUnk)
	{
		pUnk->QueryInterface( IID_IMWContext, (LPVOID *) &m_pIMWContext );
		m_pPageProxy->SetContext(pUnk);
	}
}

LRESULT CURLBar::WindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{
    if ( ( message == WM_COMMAND ) &&
#ifdef WIN32
        ( LOWORD( wParam ) == IDC_URL_EDIT ) )
            switch ( HIWORD ( wParam ) )
#else
        ( wParam == IDC_URL_EDIT ) )
            switch ( HIWORD ( lParam ) )      
#endif
    {
        case CBN_EDITCHANGE:
            OnEditChange ( );
            break;

        case CBN_SELCHANGE:
            OnSelChange ( );
            Invalidate(FALSE);
            break;

        case CBN_CLOSEUP:
            Invalidate(FALSE);
            break;

    }
    return CURLBarBase::WindowProc ( message, wParam, lParam );
}


void CURLBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialogBar::OnLButtonDown(nFlags, point);

    int load = FALSE;

    if (!m_pIMWContext) 
        return;

	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));


}

HBRUSH CURLBar::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
  return CURLBarBase::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CURLBar::OnPaint() 

{
    CURLBarBase::OnPaint ( );

	CClientDC dc(this);

	CRect rcClient;

	GetClientRect(&rcClient);
	
	HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
	HPALETTE hOldPalette = ::SelectPalette(dc.m_hDC, hPalette, FALSE);

	// Use our background color
	::FillRect(dc.m_hDC, &rcClient, sysInfo.m_hbrBtnFace);

#ifdef _WIN32
	if ( sysInfo.m_bWin4 )
		m_pBox->SetFont( CFont::FromHandle ( (HFONT) GetStockObject (DEFAULT_GUI_FONT) ) );
	else 
#endif
	if ( !sysInfo.m_bDBCS )
		m_pBox->SetFont ( CFont::FromHandle ( (HFONT) GetStockObject ( ANSI_VAR_FONT ) ) );
	else
	{
		ASSERT(m_pFont != NULL);
		::SendMessage(m_pBox->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_pFont, FALSE);
	}
    m_pBox->UpdateWindow ( );

     if ( !sysInfo.m_bWin4 )
     {
		 // Draw 3D frame around URL box

	    RECT rect;
		m_pBox->GetWindowRect ( &rect );
        rect.right++;
        rect.bottom++;
        rect.left--;
        rect.top--;

		ScreenToClient ( &rect );
		rect.right--;
	    rect.bottom--;

		RECT rcTmp = rect;
		::InflateRect( &rcTmp, -1, -1 );
	
		WFE_DrawHighlight( dc.m_hDC, &rcTmp, 
				   RGB(128, 128, 128), 
				   RGB(0, 0, 0));
		WFE_DrawHighlight( dc.m_hDC, &rect, 
				   RGB(192, 192, 192), 
				   RGB(231, 231, 231) );
	}

	::SelectPalette(dc.m_hDC, hOldPalette, TRUE);

}

void CURLBar::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CURLBar::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}


#define TOTAL_SAVED_URLS        15
#define TOTAL_DISPLAYED_URLS    10

int CURLBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	int nRtn = CURLBarBase::OnCreate(lpCreateStruct);

    if (nRtn != -1)
	{
		CRect rect(0,0,0,0);

		// Create the proxy container
		m_pProxySurround = new CProxySurroundWnd();
		m_pProxySurround->Create(this);

		// Create the page proxy
		m_pPageProxy = new CPageProxyWindow;
		m_pPageProxy->Create(m_pProxySurround);

		// Create the edit box
		m_pBox = new CEditWnd(this);
		m_pBox->Create(
	        WS_CHILD|WS_VISIBLE|WS_TABSTOP,
			rect, (CWnd *) m_pProxySurround, IDC_URL_EDIT);
		
	}

    return(nRtn);
}

void CURLBar::OnDestroy( )
{
	CWnd::OnDestroy();
}
  

void CURLBar::OnClose()
{
}

void CURLBar::OnPaletteChanged( CWnd* pFocusWnd )
{
	if (pFocusWnd != this) {
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
		if (WFE_IsGlobalPalette(hPalette)) {
			HDC hDC = ::GetDC(m_hWnd);
			HPALETTE hOldPalette = ::SelectPalette(hDC, hPalette, FALSE);
			::SelectPalette(hDC, hOldPalette, TRUE);
			::ReleaseDC(m_hWnd, hDC);
		}
		Invalidate();
	}
}



CURLBar::~CURLBar()
{
    delete m_pBox;
	if (m_pFont) {
		theApp.ReleaseAppFont(m_pFont);
	}
	delete m_pPageProxy;

	::DeleteObject(m_hBackgroundBrush);
    // decrease reference count
    // if we are the last one delete all of the bitmaps
    if(0 == --numberOfUrlBars) {
		delete pDragURLBitmap;
		pDragURLBitmap = NULL;
    }

}

void CURLBar::DoDataExchange(CDataExchange* pDX)
{
    CURLBarBase::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CURLBar)
    //}}AFX_DATA_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CURLBar message handlers

void CURLBar::OnSize(UINT nType, int cx, int cy)
{
 
    int txtSpacing;
    int boxSpacing;
    CRect rcURLtext, rcIcon, bookmarkRect;
	int x = BOOKMARKS_TEXT_LEFT;
	
	CWnd *pText = GetDlgItem( IDC_URLTEXT );

    if (pText) {
        pText->GetWindowRect(&rcURLtext);
        // figure out how big we need to make the box
        CDC * pDC = m_pBox->GetDC();
        
        TEXTMETRIC tm;

		// creates font for multibyte system running on WinNT and Win16 
		// on Win95, m_pFont should point to DEFAULT_GUI_FONT
		// on single byte NT or win16, it should point to ANSI_VAR_FONT
#ifdef XP_WIN32
		if (sysInfo.m_bWin4 == TRUE)
			m_pBox->SetFont( CFont::FromHandle ( (HFONT) GetStockObject (DEFAULT_GUI_FONT) ) );
		else 
#endif
		if (sysInfo.m_bDBCS == FALSE)
			m_pBox->SetFont ( CFont::FromHandle ( (HFONT) GetStockObject ( ANSI_VAR_FONT ) ) );
		else {
			if (m_pFont == 0) {
				LOGFONT lf;
				XP_MEMSET(&lf,0,sizeof(LOGFONT));
				lf.lfPitchAndFamily = FF_SWISS;
				lf.lfCharSet = IntlGetLfCharset(0);
   				lf.lfHeight = -MulDiv(9,pDC->GetDeviceCaps(LOGPIXELSY), 72);
				strcpy(lf.lfFaceName, IntlGetUIPropFaceName(0));
				lf.lfWeight = 400;
				m_pFont = theApp.CreateAppFont( lf );
			}
			::SendMessage(m_pBox->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_pFont, FALSE);
		}

        pDC->GetTextMetrics(&tm);
        // stay inside of the space we've been given                                   
        m_nBoxHeight = tm.tmHeight; // + tm.tmDescent;
        if(m_nBoxHeight > (cy - 2))
            m_nBoxHeight = cy - 2;
        m_pBox->ReleaseDC(pDC);       
    }

    CURLBarBase::OnSize(nType, cx, cy);

    if( pText && m_pBox ) 
	{
        // position the "Location" text 
		txtSpacing = (cy - rcURLtext.Height()) / 2 ; 

        pText->MoveWindow(x, 
                          txtSpacing, rcURLtext.Width(), rcURLtext.Height());

		x += rcURLtext.Width() + 3;

		m_DragIconX = x;

		// Position the proxy surround wnd
		
		m_pProxySurround->MoveWindow(x-1, 1, ( cx - x - 
                        ( GetSystemMetrics(SM_CXBORDER) * 2)),cy-2);

        boxSpacing = (cy - m_nBoxHeight) / 2 ;
        m_DragIconY = boxSpacing + (m_nBoxHeight - DRAG_ICON_HEIGHT)/2;

		m_pPageProxy->MoveWindow(1, m_DragIconY-1, DRAG_ICON_WIDTH, DRAG_ICON_HEIGHT);
		
		x += DRAG_ICON_WIDTH + 2;

        // position the location edit box
        m_pBox->MoveWindow( DRAG_ICON_WIDTH+3,
                   (cy - m_nBoxHeight) / 2 -1, 
                   ( cx - x - 
                        ( GetSystemMetrics(SM_CXBORDER) * 2)) ,
						m_nBoxHeight); 
	}
      
    m_DragIconY = boxSpacing + (m_nBoxHeight - DRAG_ICON_HEIGHT)/2 + 1;
	
	
}

//////////////////////////////////////////////////////////////////////////////
// The user has hit return in the URL box.  download the current selection and
// give the focus back to the frame
void CURLBar::ProcessEnterKey() 
{
    char *new_text = NULL;
    char    text[1000];
    URL_Struct * URL_s;

    if (!m_pIMWContext) return;

    m_pBox->GetWindowText(text, 1000);

    if (text[0])
    {
        MWContext *context = m_pIMWContext->GetContext();
        int32 id;

        if (context)
        {
            id = XP_CONTEXTID(context);
        }
        else
        {
            id = 0;
        }

        if (HK_CallHook(HK_LOCATION, NULL, id, text, &new_text))
        {
            // Nothing, new_text is already set.
        }
        else
        {
            new_text = NULL;
        }
    }

    if (new_text)
    {
        URL_s = NET_CreateURLStruct(new_text, NET_DONT_RELOAD);
        XP_FREE(new_text);
    }
    else
    {
        URL_s = NET_CreateURLStruct(text, NET_DONT_RELOAD);
    }

	//	Load up.
	m_pIMWContext->GetUrl(URL_s, FO_CACHE_AND_PRESENT);
}


//////////////////////////////////////////////////////////////////////////////////////
// Set the current url bar to the given string and make sure that the text label
//   says "Location:".  We are probably setting it more frequently than we
//   need to because of the location / netsite history
void CURLBar::UpdateFields( const char * msg )
{       
    CWnd *pText = GetDlgItem( IDC_URLTEXT );

    // strip random backend crap out of the url
    msg = LM_StripWysiwygURLPrefix(msg);

    CString cs(msg);
    m_pBox->SetWindowText(cs); 
    UpdateWindow();
    //m_pBox->SetEditSel(cs.GetLength()-1,cs.GetLength());
    m_pBox->Clear();
    
	// If this was a netsite server change the string
	History_entry*	pEntry = SHIST_GetCurrent(&m_pIMWContext->GetContext()->hist);

	if (pEntry && pEntry->is_netsite)
		pText->SetWindowText(szLoadString(IDS_NETSITE));
	else
		pText->SetWindowText(szLoadString(IDS_LOCATION));

    m_nTextStatus = TRUE;
}

void CURLBar::SetToolTip(const char *pTip)
{
	m_pBox->SetToolTip(pTip);
}

void CURLBar::OnEditCopy()
{
    m_pBox->Copy();
    OnEditChange();
}

void CURLBar::OnUpdateEditCopy(CCmdUI* pCmdUI)	{
	int iStartSel, iEndSel;
	m_pBox->GetSel(iStartSel, iEndSel);

	if(iStartSel != iEndSel)	{
		pCmdUI->Enable(TRUE);
	}
	else	{
		pCmdUI->Enable(FALSE);
	}
}

void CURLBar::OnEditCut()
{
    m_pBox->Cut();
    OnEditChange();
}

void CURLBar::OnUpdateEditCut(CCmdUI* pCmdUI)	{
	int iStartSel, iEndSel;
	m_pBox->GetSel(iStartSel, iEndSel);

	if(iStartSel != iEndSel)	{
		pCmdUI->Enable(TRUE);
	}
	else	{
		pCmdUI->Enable(FALSE);
	}
}

void CURLBar::OnEditPaste()
{

	m_pBox->Paste();
    OnEditChange();
}

void CURLBar::OnUpdateEditPaste(CCmdUI* pCmdUI)	{
	pCmdUI->Enable(::IsClipboardFormatAvailable(CF_TEXT));
}

void CURLBar::OnEditUndo()
{
    m_pBox->Undo();
    OnEditChange();
}

void CURLBar::OnUpdateEditUndo(CCmdUI* pCmdUI)	{
	pCmdUI->Enable(m_pBox->CanUndo());
}

void CURLBar::OnSelChange()
{
}

///////////////////////////////////////////////////////////////////////////////////////
// The user has modified the text in the edit field.  make sure it is now labeled as
//   "Go to:" rather than "Location:"
void CURLBar::OnEditChange()
{
	CWnd *pText = GetDlgItem( IDC_URLTEXT );

    if( pText && m_nTextStatus )
    {
        pText->SetWindowText(szLoadString(IDS_GOTO));
        m_nTextStatus = FALSE;
    }
}

void CURLBar::OnMouseMove(UINT nFlags, CPoint point) 
{
	CURLBarBase::OnMouseMove(nFlags, point);
    // Display previous status
    wfe_Progress( m_pIMWContext->GetContext(), "" );

  	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x, point.y));

}

void CURLBar::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	CURLBarBase::OnLButtonDblClk(nFlags, point);

}


void CURLBar::OnLButtonUp(UINT nFlags, CPoint point) 
{
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_LBUTTONUP, nFlags, MAKELPARAM(point.x, point.y));

}

CEditWnd::~CEditWnd()
{
    if (m_idTimer)
        KillTimer(m_idTimer);
    if (m_pComplete)
        free(m_pComplete);
    delete m_ToolTip;
}


BEGIN_MESSAGE_MAP(CEditWnd,CGenericEdit)
    ON_WM_TIMER()
    ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()


LRESULT CEditWnd::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = CEdit::DefWindowProc(message,wParam,lParam);
    if (message == WM_CHAR)
    {
        switch (wParam)
        {
            case VK_BACK:
                break;
            default:
                UrlCompletion();
                break;
        }
    }
    return result;
}

#define URL_AUTO_CHECK_DELAY    25
#define URL_TIMER 10

BOOL CEditWnd::PreTranslateMessage ( MSG * msg )
{
   if ( msg->message == WM_KEYDOWN)
   {
	  CURLBar* pBar = (CURLBar*)m_pBar;
	  LPMWCONTEXT pCX = pBar->GetContext();
      
	  switch(msg->wParam)
      {
         case VK_RETURN:
			 // Kill any urlcompletion still on the timer
			 m_bRestart = TRUE;
			 if (m_idTimer)
			 {
				KillTimer(m_idTimer);
				m_idTimer = 0;
			 }
                  
             if (pCX) 
             {
                CString url;
                char *new_url = NULL;
                char *url_str;
                GetWindowText ( url );

				// Call the location_hook to process and possibly change the
				// text just entered.
        
				if ((url)&&(*url))
				{
					MWContext *context = (MWContext *)pCX;
					int32 id;

					if (context)
					{
						id = XP_CONTEXTID(context);
					}
					else
					{
						id = 0;
					}

					const char *c_url = (const char *)url;
					url_str = (char *)c_url;
					if (HK_CallHook(HK_LOCATION, NULL, id, url_str, &new_url))
					{
						// Nothing, new_url is already set.
					}
					else
					{
						new_url = NULL;
					}
				}
			
				if (new_url)
				{
					if ( strlen ( new_url ) )
					{
						CNSGenFrame* pFrame = (CNSGenFrame*)GetTopLevelFrame();
						BOOL doNormalLoad = TRUE;
						if (pFrame)
						{
							CNSNavFrame* pNavCenter = pFrame->GetDockedNavCenter();
							if (pNavCenter)
							{
								HT_Pane thePane = pNavCenter->GetHTPane();
								doNormalLoad = !HT_LaunchURL(thePane, new_url, pCX->GetContext());
							}
						}
						
						if (doNormalLoad)
							pCX->NormalGetUrl(new_url);
					}
					XP_FREE(new_url);
				}
				else
				{
					if ( strlen ( url ) )
					{
						CNSGenFrame* pFrame = (CNSGenFrame*)GetTopLevelFrame();
						BOOL doNormalLoad = TRUE;
						if (pFrame)
						{
							CNSNavFrame* pNavCenter = pFrame->GetDockedNavCenter();
							if (pNavCenter)
							{
								HT_Pane thePane = pNavCenter->GetHTPane();
								doNormalLoad = !HT_LaunchURL(thePane, (char*)(const char*)url, pCX->GetContext());
							}
						}
						
						if (doNormalLoad)
							pCX->NormalGetUrl(url);
					}
				}
			 }
			 break;
		 case VK_DOWN:
            {
               int nStart, nEnd;
               CString csOriginal, csPartial;
               GetWindowText(csOriginal);
               GetSel(nStart,nEnd);
               if (nEnd>=nStart)
               {
                  csPartial = csOriginal.Left(nStart);
                  char * pszResult = NULL;
                  switch (urlMatch(csPartial,&pszResult,FALSE, m_Scroll = TRUE))
                  {
                     case stillSearching:
                        if (!m_idTimer)
                        {
                           m_idTimer = SetTimer(URL_TIMER, URL_AUTO_CHECK_DELAY, NULL);
                           m_bRestart = FALSE;
                        }
                        break;
                  }
                  DrawCompletion(csPartial,pszResult);
               }
            }
			return TRUE;
		default:
            m_Scroll = FALSE;
            break;
      }
	}
   return CEdit::PreTranslateMessage ( msg );
}

void CEditWnd::UrlCompletion()
{
    CString cs;
	Bool cursorAtEnd, textSelected;
	int nStart, nEnd;
    GetWindowText(cs);
	GetSel(nStart,nEnd);
    char * pszResult = NULL;


	textSelected = (nEnd - nStart) ? TRUE : FALSE;
	cursorAtEnd = (!textSelected && ( nEnd == cs.GetLength() ));

	if(textSelected || cursorAtEnd)
	{
		switch (urlMatch(cs,&pszResult,m_bRestart, m_Scroll = FALSE))
		{
			case dontCallOnIdle:
			case notFoundDone:
			case foundDone:
				m_bRestart = TRUE;
				if (m_idTimer)
				{
					KillTimer(m_idTimer);
					m_idTimer = 0;
				}
				break;
			case stillSearching:
				m_bRestart = FALSE;
				if (!m_idTimer)
				{
					m_idTimer = SetTimer(URL_TIMER, URL_AUTO_CHECK_DELAY, NULL);
					break;
				}
				break;
		}
    
		DrawCompletion(cs, pszResult);
	}
}

void CEditWnd::SetToolTip(const char *inTipStr)
{
	// no tooltip created, and no tooltip to set.  ok fine.
	if (!m_ToolTip && (!inTipStr || !*inTipStr))
		return;

	if (!m_ToolTip) {
		m_ToolTip = new CNSToolTip2();
		if (m_ToolTip) {
			m_ToolTip->Create(this, TTS_ALWAYSTIP);
			m_ToolTip->SetDelayTime(200);
			m_ToolTip->AddTool(this, "");
		}
	}
	if (m_ToolTip)
		if (!inTipStr || !*inTipStr)
			m_ToolTip->UpdateTipText("", this);
		else
			m_ToolTip->UpdateTipText(inTipStr, this);
}

void CEditWnd::DrawCompletion(CString & cs, char * pszResult)
{
    if (pszResult)
    {
        if (m_pComplete)
        {
            free(m_pComplete);
            m_pComplete = NULL;
        }
        m_pComplete = strdup(pszResult);
        UINT iPre = cs.GetLength();
        CString csNew;
        csNew = cs;
        if (iPre >= 0 && iPre <= strlen(pszResult))
            csNew += CString(&pszResult[iPre]);
        SetWindowText(csNew);
        SetSel((int)iPre,-1);
		free(pszResult);
		pszResult = NULL;
    }
}

void CEditWnd::OnTimer( UINT  nIDEvent )
{
   if (nIDEvent == m_idTimer)
   {
      int nStart, nEnd;
      CString csOriginal, csPartial;
      GetWindowText(csOriginal);
      GetSel(nStart,nEnd);
      if ( (nEnd>=nStart) && m_Scroll )
         csPartial = csOriginal.Left(nStart);
      else
         csPartial = csOriginal;
      char * pszResult = NULL;
      switch (urlMatch(csPartial,&pszResult,m_bRestart, m_Scroll))
      {
         case dontCallOnIdle:
	         m_bRestart = FALSE;
             if (m_idTimer)
             {
                 KillTimer(m_idTimer);
                 m_idTimer = 0;
             }
             break;
         case notFoundDone:
         case foundDone:
             m_bRestart = TRUE;
             if (m_idTimer)
             {
                 KillTimer(m_idTimer);
                 m_idTimer = 0;
             }
             break;
      }
      DrawCompletion(csPartial, pszResult);
    }
}

void CEditWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	CGenericEdit::OnMouseMove(nFlags, point);

	if (m_ToolTip) {
		m_ToolTip->Activate(TRUE);
		MSG msg = *(GetCurrentMessage());
		m_ToolTip->RelayEvent(&msg);
	}
}


// CProxySurroundWnd

BEGIN_MESSAGE_MAP(CProxySurroundWnd, CWnd) 
    //{{AFX_MSG_MAP(CProxySurroundWnd)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CProxySurroundWnd::Create(CWnd *pParent)
{
	CRect rect(0, 0, 10, 10); // will size/position it later

	BOOL bRtn = CWnd::CreateEx( NULL, 
                                NULL, 
                                NULL, 
                                WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | WS_BORDER, 
                                0, 0, 10, 10,
                        		pParent ? pParent->m_hWnd : NULL, 0 );
	return bRtn;
}

void CProxySurroundWnd::OnPaint() 
{
    CWnd::OnPaint ( );

	CClientDC dc(this);

	CRect rcClient;

	GetClientRect(&rcClient);
	
	// Use window background color
	HBRUSH br = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
	::FillRect(dc.m_hDC,&rcClient,br);
	::DeleteObject(br);

}

///////////////////////////////////////////////////////////////////////////////////////////
//								Class CPageProxyWindow
///////////////////////////////////////////////////////////////////////////////////////////

#define IDT_PROXYFOCUS 16420
#define PROXYFOCUS_DELAY_MS 10

CPageProxyWindow::CPageProxyWindow(void)
{
	m_bDraggingURL = FALSE;
	m_bDragIconHit = FALSE;
	m_bDragStatusHint = FALSE;
	m_pIMWContext = NULL;
}

CPageProxyWindow::~CPageProxyWindow(void)
{


}
void CPageProxyWindow::SetContext( LPUNKNOWN pUnk )
{
	if (m_pIMWContext) {
		m_pIMWContext->Release();
		m_pIMWContext = NULL;
	}
	if (pUnk)
		pUnk->QueryInterface( IID_IMWContext, (LPVOID *) &m_pIMWContext );
}

BOOL CPageProxyWindow::Create(CWnd *pParent)
{
	CRect rect(0, 0, 10, 10); // will size/position it later

	BOOL bRtn = CWnd::CreateEx( NULL, 
                                AfxRegisterWndClass( CS_SAVEBITS, theApp.LoadCursor( IDC_HANDOPEN ), 
                                                     (HBRUSH)(COLOR_BTNFACE+1) ), 
                                NULL, 
                                WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, 
                                0, 0, 10, 10,
                        		pParent ? pParent->m_hWnd : NULL, 0 );
	if(bRtn)
	{
		CString tipStr;

		tipStr.LoadString(IDS_DRAGURLTIP);

		
		m_ToolTip.Create(this, TTS_ALWAYSTIP);


		m_ToolTip.AddTool(this, (const char*) tipStr );

		m_ToolTip.Activate(TRUE);
		m_ToolTip.SetDelayTime(200);
	
	}

	return bRtn;
}

///////////////////////////////////////////////////////////////////////////////////////////
//								CPageProxyWindow Messages
///////////////////////////////////////////////////////////////////////////////////////////

int CPageProxyWindow::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{

    return(0);
}

BEGIN_MESSAGE_MAP(CPageProxyWindow, CWnd) 
    //{{AFX_MSG_MAP(CPageProxyWindow)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONUP()
	ON_WM_TIMER()
    ON_WM_MOUSEACTIVATE()

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CPageProxyWindow::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	CWnd::OnLButtonDblClk(nFlags, point);

    if ( m_pIMWContext && !m_pIMWContext->GetContext()->waitingMode && 
         !m_bDraggingURL && !m_bDragIconHit)  {
		WINCX(m_pIMWContext->GetContext())->CopyCurrentURL();
    }
}


void CPageProxyWindow::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
    m_bDragIconHit = FALSE;
	MSG msg = *(GetCurrentMessage());
	m_ToolTip.RelayEvent(&msg);

	SetCursor(theApp.LoadCursor(IDC_HANDOPEN));

}

int CPageProxyWindow::OnMouseActivate( CWnd *, UINT, UINT )
{
    //
    // Prevent the frame window from activating.  Allows for easier drag/drop from proxy
    // to a different top-level window in that the browser frame retains it's z-position.
    //
    return MA_NOACTIVATE;
}

void CPageProxyWindow::OnMouseMove(UINT nFlags, CPoint point) 
{
	CWnd::OnMouseMove(nFlags, point);

	SetCursor(theApp.LoadCursor(IDC_HANDOPEN));

	if ( m_pIMWContext && !m_pIMWContext->GetContext()->waitingMode && !m_bDraggingURL ) {
        if ( m_bDragIconHit &&
             ((abs(point.x - m_cpLBDown.x) > 3)
             || (abs(point.y - m_cpLBDown.y) > 3)) ) {
            // We moved enough to start dragging
            m_bDraggingURL = TRUE;
            // DragNDrop baby!
			m_bDragStatusHint = FALSE;
			RedrawWindow();
			ReleaseCapture();
            WINCX(m_pIMWContext->GetContext())->DragCurrentURL();
            // Done -- clear flags
            m_bDraggingURL = FALSE;
            m_bDragIconHit = FALSE;

            // Return focus to frame??
            // Not such a good idea; Drag/drop ops should maintain z-order.
            // GetTopLevelFrame()->SetFocus();
       } else if( (point.x >= 0) && 
                   (point.x <=  DRAG_ICON_WIDTH) &&
                   (point.y >= 0) && 
                   (point.y <=  DRAG_ICON_HEIGHT) ) {
            // Mouse is over the icon and button is up
            // TODO: Simulate ToolTip here (IDS_DRAG_THIS_URL_TIP)
            // For now -- display on StatusBar
            if ( !m_bDragStatusHint ) {
                m_pIMWContext->Progress(m_pIMWContext->GetContext(), szLoadString(ID_DRAG_THIS_URL));
                m_bDragStatusHint = TRUE;
				m_hFocusTimer = SetTimer(IDT_PROXYFOCUS, PROXYFOCUS_DELAY_MS, NULL);
				RedrawWindow();
	            }
       } else if (m_bDragStatusHint) {
            // Restore message to previous contents
          #if 0
            m_pIMWContext->Progress(m_pIMWContext->GetContext(), "");
          #else
            // Display previous status
            wfe_Progress( m_pIMWContext->GetContext(), "" );
          #endif
            m_bDragStatusHint = FALSE;
			RedrawWindow();

        }
    }

	m_ToolTip.Activate(TRUE);
	MSG msg = *(GetCurrentMessage());
	m_ToolTip.RelayEvent(&msg);
}

void CPageProxyWindow::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

    int load = FALSE;

    if (!m_pIMWContext) 
        return;

	m_cpLBDown = point;
    
    //CLM: Check if clicking in the Drag icon:
    if (!m_pIMWContext->GetContext()->waitingMode && (point.x >= 0) && 
        (point.x <=  DRAG_ICON_WIDTH) &&
        (point.y >= 0) && 
        (point.y <=  DRAG_ICON_HEIGHT)) {
        m_bDragIconHit = TRUE;
		SetCursor(theApp.LoadCursor(IDC_LINK_COPY));

		SetCapture();
    }

	MSG msg = *(GetCurrentMessage());
	m_ToolTip.RelayEvent(&msg);


}

void CPageProxyWindow::OnPaint() 
{
    CWnd::OnPaint ( );

	CClientDC dc(this);

	CRect rcClient;

	GetClientRect(&rcClient);
	
	// Use window background color
	HBRUSH br = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
	::FillRect(dc.m_hDC,&rcClient,br);
	::DeleteObject(br);

    CDC SrcDC;
	SrcDC.CreateCompatibleDC(&dc);


    // Draw the bitmap for dragging this URL

    // Create the bitmaps if we haven't already
    if(pDragURLBitmap == NULL) {
        pDragURLBitmap = new CBitmap;

        if(pDragURLBitmap) {
			pDragURLBitmap->LoadBitmap( IDB_DRAG_URL );
        }
    }

    if(pDragURLBitmap) {
		CPoint bitmapStart(m_bDragStatusHint ? DRAG_ICON_WIDTH : 0 , 0);

        CBitmap *pOldBitmap = SrcDC.SelectObject(pDragURLBitmap);
		::FEU_TransBlt( &SrcDC, &dc,
						bitmapStart, 
						CPoint(0, 0),
						DRAG_ICON_WIDTH, DRAG_ICON_HEIGHT,WFE_GetUIPalette(GetParentFrame()));

        SrcDC.SelectObject( pOldBitmap );
    }


	SrcDC.DeleteDC();


}

void CPageProxyWindow::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CPageProxyWindow::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}

void CPageProxyWindow::OnTimer( UINT  nIDEvent )
{
	if(nIDEvent == IDT_PROXYFOCUS)
	{
		POINT point;

		KillTimer(IDT_PROXYFOCUS);
		m_hFocusTimer = 0;
		GetCursorPos(&point);

		CRect rcClient;
		GetWindowRect(&rcClient);

		if (!rcClient.PtInRect(point))
		{
			m_bDragStatusHint = FALSE;
			RedrawWindow();
		
		}
		else
			m_hFocusTimer = SetTimer(IDT_PROXYFOCUS, PROXYFOCUS_DELAY_MS, NULL);

	}


}
