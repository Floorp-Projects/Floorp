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

// Created August 19, 1996 by Scott Putterman

#include "stdafx.h"
#include "tlbutton.h"
#include "mainfrm.h"
#include "ipframe.h"

#define TEXT_CHARACTERS_SHOWN 9
#define BORDERSIZE 2
#define TEXTVERTMARGIN 1
#define TEXTONLYVERTMARGIN 2
#define BITMAPVERTMARGIN 2
#define TEXT_BITMAPVERTMARGIN 1
#define HORIZMARGINSIZE 4

#define IDT_MENU		16383
#define MENU_DELAY_MS	500

#define IDT_BUTTONFOCUS 16410
#define BUTTONFOCUS_DELAY_MS 10

static CToolbarButton *pCurrentButton = NULL;

void WFE_ParseButtonString(UINT nID, CString &statusStr, CString &toolTipStr, CString &textStr)
{

		CString str1;

		str1.LoadString(nID);

		int nNewLineIndex = str1.Find('\n');
	
		if(nNewLineIndex == -1)
		{
			statusStr = str1;
			toolTipStr = statusStr;
			textStr = statusStr;
		}
		else
		{
			statusStr = str1.Left(nNewLineIndex);
			str1 = str1.Right( str1.GetLength() - (nNewLineIndex + 1));

			nNewLineIndex = str1.Find('\n');
			if( nNewLineIndex == -1)
			{
				toolTipStr = str1;
				textStr = toolTipStr;
			}
			else
			{
				toolTipStr = str1.Left(nNewLineIndex);
				textStr = str1.Right(str1.GetLength() - (nNewLineIndex + 1));
			}
		}



}

CButtonEditWnd::~CButtonEditWnd()
{

}


void CButtonEditWnd::SetToolbarButtonOwner(CToolbarButton *pOwner)
{
	m_pOwner = pOwner;
}

BOOL CButtonEditWnd::PreTranslateMessage ( MSG * msg )
{
   if ( msg->message == WM_KEYDOWN)
   {
      switch(msg->wParam)
      {
         case VK_RETURN:
         {
			m_pOwner->EditTextChanged(m_pOwner->GetTextEditText());
            return TRUE; // Bail out, since m_pOwner has been deleted.

         }
		 case VK_ESCAPE:
			 m_pOwner->RemoveTextEdit();
			 break;
	  }
   }
   
   return CEdit::PreTranslateMessage ( msg );
}

//////////////////////////////////////////////////////////////////////////
//					Messages for CButtonEditWnd
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CButtonEditWnd, CEdit)
	//{{AFX_MSG_MAP(CWnd)
 	ON_WM_DESTROY()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CButtonEditWnd::OnDestroy( )
{
	delete this;

}
  
void CButtonEditWnd::OnKillFocus( CWnd* pNewWnd )
{
	 m_pOwner->RemoveTextEdit();
}

////////////////////////////////////////////////////

void CALLBACK EXPORT NSButtonMenuTimerProc(
    HWND hwnd,	// handle of window for timer messages 
    UINT uMsg,	// WM_TIMER message
    UINT idEvent,	// timer identifier
    ULONG dwTime 	// current system time
    );

void CToolbarButtonCmdUI::Enable(BOOL bOn)
{

	if(bOn != ((CToolbarButton*)m_pOther)->IsEnabled())
	{

		((CToolbarButton*)m_pOther)->Enable(bOn);

		m_pOther->Invalidate();
	}

	m_bEnableChanged = TRUE;
}

void CToolbarButtonCmdUI::SetCheck( int nCheck)
{


	if( nCheck != ((CToolbarButton*)m_pOther)->GetCheck())
	{
		((CToolbarButton*)m_pOther)->SetCheck(nCheck);
		m_pOther->Invalidate();

	}


}
 

DROPEFFECT CToolbarButtonDropTarget::OnDragEnter(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn;

	// Only interested in bookmarks
	deReturn = ProcessDragEnter(pWnd, pDataObject, dwKeyState, point);
	if(deReturn != DROPEFFECT_NONE)
		pWnd->SendMessage(NSDRAGGINGONBUTTON, 0, 0);

	return(deReturn);

} 


DROPEFFECT CToolbarButtonDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	
	return ProcessDragOver(pWnd, pDataObject, dwKeyState, point);
} 


BOOL CToolbarButtonDropTarget::OnDrop(CWnd * pWnd,
	COleDataObject * pDataObject, DROPEFFECT dropEffect, CPoint point)
{

	return ProcessDrop(pWnd, pDataObject, dropEffect, point);
	
} 

CToolbarButton::CToolbarButton()
{
	m_nIconType = BUILTIN_BITMAP;
	m_pBitmapFile = NULL;
	m_pButtonText = NULL;
	m_pToolTipText = NULL;
	m_pStatusText = NULL;
	m_bMenuShowing =FALSE;
	m_bButtonDown = FALSE;
	m_bHaveFocus = FALSE;
	m_bIsResourceID = TRUE;
	m_hFocusTimer = 0;
	m_pDropMenu = NULL;
	m_pDropTarget = NULL;
	m_bDraggingOnButton = FALSE;
	m_hBmpImg = NULL;
	m_nChecked = 0;
	m_bPicturesOnly = FALSE;
    m_bDoOnButtonDown = FALSE;
    m_dwButtonStyle = 0;
	m_bDepressed = FALSE;
	hasCustomTextColor = FALSE;
	hasCustomBGColor = FALSE;
}


CToolbarButton::~CToolbarButton()
{
	if(m_pButtonText != NULL)
	{
		XP_FREE(m_pButtonText);
	}

	if(m_pToolTipText != NULL)
	{
		XP_FREE(m_pToolTipText);
	}

	if(m_pStatusText != NULL)
	{
		XP_FREE(m_pStatusText);
	}

	if(m_pBitmapFile != NULL)
	{
		XP_FREE(m_pBitmapFile);
	}

	if(m_pDropMenu != NULL)
	{
		delete m_pDropMenu;
	}

	if(m_pDropTarget != NULL)
	{
		delete m_pDropTarget;
	}

	if(m_hBmpImg != NULL && !m_bBitmapFromParent)
		DeleteObject(m_hBmpImg);

	if(pCurrentButton == this)
		pCurrentButton = NULL;
}

int CToolbarButton::Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize,
						   CSize advancedButtonSize,  LPCTSTR pButtonText, LPCTSTR pToolTipText,
						   LPCTSTR pStatusText, UINT nBitmapID, UINT nBitmapIndex, CSize bitmapSize, 
						   BOOL bNeedsUpdate, UINT nCommand, int nMaxTextChars, int nMinTextChars,
                           DWORD dwButtonStyle)
{
	


	m_nBitmapID = nBitmapID;
	m_nBitmapIndex = nBitmapIndex;
	m_nToolbarStyle = nToolbarStyle;
	m_noviceButtonSize = noviceButtonSize;
	m_advancedButtonSize = advancedButtonSize;
	m_bitmapSize = bitmapSize;
	m_bEnabled = TRUE;
	m_bNeedsUpdate = bNeedsUpdate;
	m_nCommand = nCommand;
	m_nMaxTextChars = nMaxTextChars;
    m_nMinTextChars = nMinTextChars;
    m_dwButtonStyle = dwButtonStyle;
    m_pTextEdit = NULL;

	CSize size = GetButtonSize();

	CRect rect(100, 100, size.cx, size.cy);
	CBrush brush;

	int bRtn = CWnd::Create( theApp.NSToolBarClass, NULL, 
					WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CS_PARENTDC,
					rect, pParent, nCommand, NULL);

	HINSTANCE hInst = AfxGetResourceHandle();

	CDC * pDC=GetDC();
	if(m_bIsResourceID)
	{
		// if we haven't been given a bitmap yet, we don't want to load this
		if(m_nBitmapID != 0)
		{
			WFE_InitializeUIPalette(pDC->m_hDC);
			HPALETTE hPalette= WFE_GetUIPalette(GetParentFrame());
			HPALETTE hOldPalette = SelectPalette(pDC->m_hDC, hPalette, FALSE);
			m_hBmpImg = WFE_LoadTransparentBitmap(hInst, pDC->m_hDC, sysInfo.m_clrBtnFace, RGB(255, 0, 255), hPalette, m_nBitmapID);
			SelectPalette(pDC->m_hDC, hOldPalette, TRUE);
		}
	}
	else
		m_hBmpImg = WFE_LoadBitmapFromFile(m_pBitmapFile);

	ReleaseDC(pDC);

	m_bBitmapFromParent = FALSE;

	SetText(pButtonText);

	if(pToolTipText != NULL)
	{
		m_pToolTipText = (LPTSTR)XP_ALLOC(XP_STRLEN(pToolTipText) + 1);
		XP_STRCPY(m_pToolTipText, pToolTipText);
	}


	m_pStatusText = (LPTSTR) XP_ALLOC(XP_STRLEN(pStatusText) + 1);
	XP_STRCPY(m_pStatusText, pStatusText);

    // Force initialization in SetStyle()
    m_dwButtonStyle = DWORD(-1);
    
    //Code moved to function so style can be set separately
    SetStyle(dwButtonStyle);

	m_eState = eNORMAL;

	return bRtn;
}

void CToolbarButton::SetStyle(DWORD dwButtonStyle)
{
    // If no change from current style, then do nothing
	if( m_dwButtonStyle == dwButtonStyle ){
        return;
    }
	m_dwButtonStyle = dwButtonStyle;
	CSize size = GetButtonSize();
    
    if(dwButtonStyle & TB_HAS_TIMED_MENU || dwButtonStyle & TB_HAS_IMMEDIATE_MENU)
	{
		// Create menu if not already done
        // Menu contents are appended and removed every time its used,
        //  so we don't need to delete items here
        if(!(dwButtonStyle & TB_HAS_DRAGABLE_MENU 
			&& m_menu.m_hMenu != NULL
			&& !IsMenu(m_menu.m_hMenu)))
			m_menu.CreatePopupMenu();
	}
    else if(dwButtonStyle & TB_HAS_DROPDOWN_TOOLBAR){
        // Even though the styles are bit flags,
        //    DROPDOWN_TOOLBAR style cannot be used with the MENU STYLES
        SetDoOnButtonDown(TRUE);   
    }

	// Create tooltip only if we have text and it wasn't created before
    if(m_pToolTipText != NULL && !IsWindow(m_ToolTip.m_hWnd))
	{
#ifdef WIN32
		m_ToolTip.Create(this, TTS_ALWAYSTIP);
#else
		m_ToolTip.Create(this);
#endif

		if(dwButtonStyle & TB_DYNAMIC_TOOLTIP)
		{
			CRect tipRect(0, 0, size.cx, size.cy);
			m_ToolTip.AddTool(this, LPSTR_TEXTCALLBACK, (m_nCommand == 0) ? NULL : &tipRect,
							  m_nCommand);
		}
		else
		{
			m_ToolTip.AddTool(this, m_pToolTipText);
		}

		m_ToolTip.Activate(TRUE);
		m_ToolTip.SetDelayTime(250);
	}
}

int CToolbarButton::Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, LPCTSTR pBitmapFile,
			   CSize m_bitmapSize, BOOL bNeedsUpdate, UINT nCommand, int nMaxTextChars, int nMinTextChars,
               DWORD dwButtonStyle)
{

	m_bIsResourceID = FALSE;

	m_pBitmapFile = (LPTSTR) XP_ALLOC(XP_STRLEN(pBitmapFile) + 1);
	XP_STRCPY(m_pBitmapFile, pBitmapFile);

	return Create(pParent, nToolbarStyle, noviceButtonSize, advancedButtonSize, pButtonText, pToolTipText,
				  pStatusText, 0, 0, m_bitmapSize, bNeedsUpdate, nCommand, nMaxTextChars, nMinTextChars,
                  dwButtonStyle);


}

void CToolbarButton::SetText(LPCTSTR pButtonText)
{
	CSize oldSize = GetRequiredButtonSize();

	if(m_pButtonText != NULL)
	{
		XP_FREE(m_pButtonText);
	}

	m_pButtonText = (LPTSTR)XP_ALLOC(XP_STRLEN(pButtonText) +1);
	XP_STRCPY(m_pButtonText, pButtonText);

	CSize newSize = GetRequiredButtonSize();

	if((m_nToolbarStyle == TB_PICTURESANDTEXT || m_nToolbarStyle == TB_TEXT) && ::IsWindow(m_hWnd))
	{
		RedrawWindow();
	}

	if(oldSize != newSize)
		GetParent()->SendMessage(TB_SIZECHANGED, 0, (LPARAM)m_hWnd);

}

void CToolbarButton::SetButtonCommand(UINT nCommand)
{
#ifdef XP_WIN32
	CToolInfo ToolInfo;
    UINT nID =  (m_dwButtonStyle & TB_DYNAMIC_TOOLTIP) ? m_nCommand: 0;
	
	m_ToolTip.GetToolInfo(ToolInfo,this,nID);
	ToolInfo.uId =   (m_dwButtonStyle & TB_DYNAMIC_TOOLTIP) ? nCommand: 0;
	m_ToolTip.SetToolInfo(&ToolInfo);
	m_nCommand = nCommand;
#else
	TOOLINFO ToolInfo;
    UINT nID =  (m_dwButtonStyle & TB_DYNAMIC_TOOLTIP) ? m_nCommand: 0;
	
	m_ToolTip.GetToolInfo(&ToolInfo);
	ToolInfo.uId =   (m_dwButtonStyle & TB_DYNAMIC_TOOLTIP) ? nCommand: 0;
	m_ToolTip.SetToolInfo(&ToolInfo);
	m_nCommand = nCommand;
#endif
}

void CToolbarButton::SetToolTipText(LPCSTR pToolTipText)

{
    if (pToolTipText == NULL) 
        return;

	if(m_pToolTipText != NULL)
	{
	    XP_FREE(m_pToolTipText);
	}
	m_pToolTipText = (LPTSTR)XP_ALLOC(XP_STRLEN(pToolTipText) +1);
	XP_STRCPY(m_pToolTipText, pToolTipText);
    UINT nID = (m_dwButtonStyle & TB_DYNAMIC_TOOLTIP) ? m_nCommand: 0;
	m_ToolTip.UpdateTipText(m_pToolTipText, this, nID);
}

void CToolbarButton::SetStatusText(LPCSTR pStatusText)
{
    if (pStatusText == NULL)
        return;

    if(m_pStatusText != NULL)
    {
		XP_FREE(m_pStatusText);
	}
	m_pStatusText = (LPTSTR)XP_ALLOC(XP_STRLEN(pStatusText) +1);
	XP_STRCPY(m_pStatusText, pStatusText);
}

void CToolbarButton::SetBitmap(HBITMAP hBmpImg, BOOL bParentOwns)
{
	if(m_hBmpImg != NULL && !m_bBitmapFromParent)
		::DeleteObject(m_hBmpImg);

	m_hBmpImg = hBmpImg;
	m_bBitmapFromParent = bParentOwns;
}

void CToolbarButton::ReplaceBitmap(UINT nBitmapID, UINT nBitmapIndex)
{
	m_nBitmapID = nBitmapID;
	m_nBitmapIndex = nBitmapIndex;

	m_bIsResourceID = TRUE;
	RedrawWindow();
}

void CToolbarButton::ReplaceBitmap(LPCTSTR pBitmapFile)
{

	if(m_pBitmapFile != NULL)
	{
		XP_FREE(m_pBitmapFile);
	}

	m_pBitmapFile = (LPTSTR)XP_ALLOC(XP_STRLEN(pBitmapFile) +1);
	XP_STRCPY(m_pBitmapFile, pBitmapFile);

	m_bIsResourceID = FALSE;
	RedrawWindow();
}

void CToolbarButton::ReplaceBitmap(HBITMAP hBmpImg, UINT nBitmapIndex, BOOL bParentOwns)
{
	SetBitmap(hBmpImg, bParentOwns);
	ReplaceBitmap(0, nBitmapIndex);
}

void CToolbarButton::ReplaceBitmapIndex(UINT nBitmapIndex)
{
	if(m_nBitmapIndex != nBitmapIndex)
	{
		m_nBitmapIndex = nBitmapIndex;
		RedrawWindow();
	}
}

CSize CToolbarButton::GetButtonSize(void)
{
	return(m_nToolbarStyle == TB_PICTURESANDTEXT ? m_noviceButtonSize : m_advancedButtonSize);
}

CSize CToolbarButton::GetRequiredButtonSize(void)
{
    return GetButtonSizeFromChars(m_pButtonText, m_nMaxTextChars);
}

CSize CToolbarButton::GetButtonSizeFromChars(CString s, int c)
{
    CSize size;

	if(m_nToolbarStyle == TB_PICTURESANDTEXT)
	{
		size = GetBitmapOnTopSize(s, c);
	}
	else if (m_nToolbarStyle == TB_PICTURES)
	{
		size = GetBitmapOnlySize();
	}
	else if(m_nToolbarStyle == TB_TEXT)
	{
		size = GetTextOnlySize(s, c);
	}

	return size;
}

CSize CToolbarButton::GetMinimalButtonSize(void)
{
    return GetButtonSizeFromChars(m_pButtonText, m_nMinTextChars);
}

CSize CToolbarButton::GetMaximalButtonSize(void)
{
    return GetButtonSizeFromChars(m_pButtonText, m_nMaxTextChars);
}

void CToolbarButton::CheckForMinimalSize(void)
{
    // A button is at its minimal size if its
    // current horizontal extent is <= its minimal horizontal extent.
    atMinimalSize = (GetButtonSize().cx <= GetMinimalButtonSize().cx);
}

void CToolbarButton::CheckForMaximalSize(void)
{
    // A button is at its maximal size if its
    // current horizontal extent is >= its maximal horizontal extent.
    atMaximalSize = (GetButtonSize().cx >= GetMaximalButtonSize().cx);
}

BOOL CToolbarButton::AtMinimalSize(void)
{
    CheckForMinimalSize();
    return atMinimalSize;
}

BOOL CToolbarButton::AtMaximalSize(void)
{
    CheckForMaximalSize();
    return atMaximalSize;
}

void CToolbarButton::SetBitmapSize(CSize sizeImage)
{
	m_bitmapSize = sizeImage;
	RedrawWindow();
}

void CToolbarButton::SetButtonSize(CSize buttonSize)
{
	if(m_nToolbarStyle == TB_PICTURESANDTEXT)
	{
		m_noviceButtonSize  = buttonSize;
	}
	else
	{
		m_advancedButtonSize = buttonSize;
	}
}

void CToolbarButton::SetDropTarget(CToolbarButtonDropTarget *pDropTarget)
{

	m_pDropTarget = pDropTarget;

	m_pDropTarget->Register(this);
	m_pDropTarget->SetButton(this);
}

void CToolbarButton::SetCheck(int nCheck)
{
	m_nChecked = nCheck;
	switch(nCheck)
	{
		case 0: m_eState = eNORMAL;
				break;
		case 1: m_eState = eBUTTON_CHECKED; //eBUTTON_DOWN;
				break;
		case 2: m_eState = eDISABLED;
				break;
	}

}

int CToolbarButton::GetCheck(void)
{
	return m_nChecked;
}

void CToolbarButton::SetPicturesOnly(int bPicturesOnly)
{
	m_bPicturesOnly = bPicturesOnly;
	SetButtonMode(TB_PICTURES);
}

void CToolbarButton::SetButtonMode(int nToolbarStyle)
{
	if(m_bPicturesOnly)
		m_nToolbarStyle = TB_PICTURES;
	else
		m_nToolbarStyle = nToolbarStyle;
}

void CToolbarButton::FillInButtonData(LPBUTTONITEM pButton)
{
	pButton->button = m_hWnd;
	
}

void CToolbarButton::ButtonDragged(void)
{
	KillTimer(m_hMenuTimer);
	m_eState = eNORMAL;
	RedrawWindow();
}

void CToolbarButton::AddTextEdit(void)
{
	if(!m_pTextEdit)
	{
		m_pTextEdit = new CButtonEditWnd;

		if(m_pTextEdit)
		{
			CRect rect;
			HDC hDC = ::GetDC(m_hWnd);
			HFONT hFont = WFE_GetUIFont(hDC);
			HFONT hOldFont = (HFONT)::SelectObject(hDC, hFont);
			TEXTMETRIC tm;

			GetTextMetrics(hDC, &tm);
			int height = tm.tmHeight;

			::SelectObject(hDC, hOldFont);
			::ReleaseDC(m_hWnd, hDC);

			GetTextRect(rect);
			if(rect.Height() > height)
			{
				rect.top += (rect.Height() - height) / 2;
				rect.bottom -= (rect.Height() - height) / 2;
			}

			BOOL bRtn = m_pTextEdit->Create(ES_AUTOHSCROLL, rect, this, 0);

			if(!bRtn)
			{
				delete m_pTextEdit;
				m_pTextEdit = NULL;
			}
			else
			{
				m_pTextEdit->ShowWindow(SW_SHOW);
				HDC hDC = ::GetDC(m_hWnd);

				HFONT hFont = WFE_GetUIFont(hDC);
				CFont *font = (CFont*) CFont::FromHandle(hFont);
				m_pTextEdit->SetFont(font);
				::ReleaseDC(m_hWnd, hDC);
				m_pTextEdit->SetFocus();
				m_pTextEdit->SetToolbarButtonOwner(this);
			}

		}

	}
}

void CToolbarButton::RemoveTextEdit(void)
{
	if(m_pTextEdit)
	{
		m_pTextEdit->PostMessage(WM_DESTROY, 0, 0);
		m_pTextEdit = NULL;

	}

}

void CToolbarButton::SetTextEditText(char *pText)
{
	if(m_pTextEdit)
	{
		m_pTextEdit->ReplaceSel(pText);
		m_pTextEdit->SetSel(0, strlen(pText), TRUE);
	}

}

// the string returned must be deleted
char * CToolbarButton::GetTextEditText(void)
{
	char str[1000];

	if(m_pTextEdit)
	{
		int size = m_pTextEdit->GetWindowText(str, 1000);
		if(size > 0)
		{
			char *newStr = new char[size + 1];
			strcpy(newStr, str);
			return newStr;
		}
	}

	return NULL;

}

void CToolbarButton::EditTextChanged(char *pText)
{
	SetText(pText);
}


//////////////////////////////////////////////////////////////////////////
//					Messages for CToolbarButton
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CToolbarButton, CWnd)
	//{{AFX_MSG_MAP(CWnd)
 	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
#ifdef _WIN32
	ON_WM_CAPTURECHANGED()
#endif
	ON_WM_GETDLGCODE()
	ON_WM_KEYDOWN()
	ON_MESSAGE(TTN_NEEDTEXT, OnToolTipNeedText) 
	ON_WM_TIMER()
	ON_MESSAGE(NSDRAGMENUOPEN, OnDragMenuOpen)
	ON_MESSAGE(NSDRAGGINGONBUTTON, OnDraggingOnButton)
	ON_WM_PALETTECHANGED()
	ON_WM_SYSCOLORCHANGE()
    ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

void CToolbarButton::OnLButtonDown(UINT nFlags, CPoint point)
{
	if(m_bEnabled)
	{
		// Show the button down look
		m_eState = eBUTTON_DOWN;
		m_bDraggingOnButton = FALSE;
		if(m_dwButtonStyle & TB_HAS_TIMED_MENU)
		{
			m_hMenuTimer = SetTimer(IDT_MENU, MENU_DELAY_MS, NULL);
		}
		else if(m_dwButtonStyle & TB_HAS_IMMEDIATE_MENU)
		{
			m_hMenuTimer = SetTimer(IDT_MENU, 0, NULL);
		}

		RedrawWindow();
	}
	if(m_pToolTipText != NULL)
	{
		MSG msg = *(GetCurrentMessage());
		m_ToolTip.RelayEvent(&msg);
	}

    // Do action immediately if this is set
	if( m_bDoOnButtonDown )
        OnAction();
    else 
        m_bButtonDown = TRUE;
}

void CToolbarButton::OnMouseMove(UINT nFlags, CPoint point)
{
	// Make sure we're activated (button look)
	if (!m_bHaveFocus)
	{
			if(GetTopLevelFrame() == GetActiveWindow() || GetTopLevelFrame() == AfxGetMainWnd())
			{
				m_bHaveFocus = TRUE;
				if(m_hFocusTimer == 0)
				{
					if(pCurrentButton)
						pCurrentButton->RemoveButtonFocus();
					pCurrentButton = this;
					m_hFocusTimer = SetTimer(IDT_BUTTONFOCUS, BUTTONFOCUS_DELAY_MS, NULL);

					BOOL bStatusText = (strcmp(m_pStatusText, "") != 0);
					char status[1000];

					if(m_dwButtonStyle & TB_DYNAMIC_STATUS){
						WFE_GetOwnerFrame(this)->SendMessage(TB_FILLINSTATUS, (WPARAM)m_nCommand, (LPARAM) status);
						WFE_GetOwnerFrame(this)->SendMessage( WM_SETMESSAGESTRING,
														   (WPARAM) 0, (LPARAM) status );
					}
					else if ( bStatusText) {
						WFE_GetOwnerFrame(this)->SendMessage( WM_SETMESSAGESTRING,
														   (WPARAM)0, (LPARAM) m_pStatusText );
					}

				}

				if(m_pToolTipText != NULL)
				{
					m_ToolTip.Activate(TRUE);
					MSG msg = *(GetCurrentMessage());
					m_ToolTip.RelayEvent(&msg);
				}

				if(m_bEnabled)
				{
					m_eState = (nFlags & MK_LBUTTON) ? eBUTTON_DOWN : eBUTTON_UP;
					RedrawWindow();
					UpdateWindow();
				}
			}
	}

	if(GetTopLevelFrame() == GetActiveWindow() || GetTopLevelFrame() == AfxGetMainWnd())
	{
		if(m_pToolTipText!= NULL)
		{
			MSG msg = *(GetCurrentMessage());
			m_ToolTip.RelayEvent(&msg);
		}
	}



}

void CToolbarButton::OnLButtonUp(UINT nFlags, CPoint point)
{
	if(m_bEnabled)
	{
		if (m_bHaveFocus)
		{
			// Do action only if not done on button down
            if(!m_bDoOnButtonDown)
            {
                // Cursor is still over the button, so back to button up look
			    m_eState = eBUTTON_UP;
			    if(m_bButtonDown)
				    OnAction();
            }
		}
		else
		{
			// Restore borderless look
			if(m_nChecked == 0)
				m_eState = eNORMAL;
		}
		
		RedrawWindow();
	}
	if(m_dwButtonStyle & TB_HAS_TIMED_MENU)
	{
		KillTimer(m_hMenuTimer);
	}
	if(m_pToolTipText != NULL)
	{
		MSG msg = *(GetCurrentMessage());
		m_ToolTip.RelayEvent(&msg);
	}
	m_bButtonDown = FALSE;

}

void CToolbarButton::OnRButtonDown(UINT nFlags, CPoint point)
{
    if (CreateRightMouseMenu())
    {
        DisplayAndTrackMenu();
    }

	if(m_pToolTipText != NULL)
	{
		MSG msg = *(GetCurrentMessage());
		m_ToolTip.RelayEvent(&msg);
	}


}

void CToolbarButton::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	int i = 0;
	i++;

}
  

BOOL CToolbarButton::OnCommand(UINT wParam,LONG lParam)
{
	return((BOOL)GetParentFrame()->SendMessage(WM_COMMAND, wParam, lParam));
}


BOOL CToolbarButton::OnNotify( WPARAM wParam, LPARAM lParam, LRESULT* pResult )
{
	NMHDR *hdr = (NMHDR*)lParam;
	if(hdr->code == TTN_NEEDTEXT)
	{
		int command = (int) wParam;
		LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam; 

		GetParentFrame()->SendMessage(TB_FILLINTOOLTIP, WPARAM(m_hWnd), lParam);
		return TRUE;
	}
#ifdef _WIN32
	else
		return (CWnd::OnNotify(wParam, lParam, pResult));
#else
	return FALSE;
#endif
}

void CToolbarButton::OnPaint()
{
	CRect updateRect;

	GetUpdateRect(&updateRect);

	CPaintDC dcPaint(this);	// device context for painting
	CRect rcClient;
	GetClientRect(&rcClient);
	
	HDC hSrcDC = dcPaint.m_hDC;

	HPALETTE hPalette;
	HPALETTE hOldPalette;
	CFrameWnd* pParent = WFE_GetOwnerFrame(this); 
	hPalette = WFE_GetUIPalette(pParent);
	hOldPalette= ::SelectPalette(hSrcDC, hPalette, FALSE);
	
	CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(pParent);
	if (pParent && pGlue && (pParent->IsKindOf(RUNTIME_CLASS(CGenericFrame)) ||
		pParent->IsKindOf(RUNTIME_CLASS(CInPlaceFrame)))) {
		CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(pParent);
		if (pGlue && !pGlue->IsBackGroundPalette()) {
			hOldPalette= ::SelectPalette(hSrcDC, hPalette, TRUE);
			::RealizePalette(hSrcDC);
		}
		else {
			hOldPalette= ::SelectPalette(hSrcDC, hPalette, FALSE);
		}
	}
	else {
		hOldPalette= ::SelectPalette(hSrcDC, hPalette, TRUE);
		::RealizePalette(hSrcDC);
	}
	HDC hMemDC = ::CreateCompatibleDC(hSrcDC);
	if(hMemDC)
	{
		HPALETTE hOldMemPalette = ::SelectPalette(hMemDC, hPalette, FALSE);

		HBITMAP hbmMem = CreateCompatibleBitmap(hSrcDC,
										rcClient.Width(),
										rcClient.Height());

		//
		// Select the bitmap into the off-screen DC.
		//

		HBITMAP hbmOld = (HBITMAP)::SelectObject(hMemDC, hbmMem);

		COLORREF rgbOldBk;
		
		// Use the button face color as our background
		HBRUSH brFace = sysInfo.m_hbrBtnFace;
		
		if (m_bDepressed)
		{
			// Use the button shadow color as our background instead.
			brFace = ::CreateSolidBrush(sysInfo.m_clrBtnShadow);
			::SetBkColor(hMemDC, sysInfo.m_clrBtnShadow);
		}
		else if (hasCustomBGColor)
		{
			// Use the button's custom background
			brFace = ::CreateSolidBrush(customBGColor);
		}
		else ::SetBkColor(hMemDC, sysInfo.m_clrBtnFace); // Use button face color.

		::FillRect(hMemDC, rcClient, brFace);
		
		if (m_bDepressed)
		{
			::DeleteObject(brFace);
		}

		// if we are not enabled then must make sure that button state is normal
		if(!m_bEnabled)
		{
			m_eState = eNORMAL;
		}

		CRect innerRect = rcClient;

		innerRect.InflateRect(-BORDERSIZE, -BORDERSIZE);

		if(m_nToolbarStyle == TB_PICTURESANDTEXT)
		{
			DrawPicturesAndTextMode(hMemDC, innerRect);
		}
		else if(m_nToolbarStyle == TB_PICTURES)
		{
			DrawPicturesMode(hMemDC, innerRect);
		}
		else
		{
			DrawTextMode(hMemDC, innerRect);
		}

		// Now, draw 3d visual button effects, depending on our state
		switch (m_eState)
		{
			case eBUTTON_UP:
			{
                if( m_nChecked == 0 )
    				DrawUpButton(hMemDC, rcClient);
                else
    			    DrawDownButton(hMemDC, rcClient);
			}
			break;
			
			case eBUTTON_CHECKED:
			{
				// A checked button but NOT mousing over - no black border
                DrawCheckedButton(hMemDC, rcClient);
			}
			break;

			case eBUTTON_DOWN:
			{
				DrawDownButton(hMemDC, rcClient);
			}
			break;

			case eDISABLED:
			{
				if(m_nChecked == 2)
					DrawCheckedButton(hMemDC, rcClient);

			}
			break;

			case eNORMAL:
			{
				if (m_bDepressed)
					DrawDownButton(hMemDC, rcClient); // Looks like it's locked down.
			}
		}

		::SetBkColor(hMemDC, rgbOldBk);

		::BitBlt(hSrcDC, 0, 0, rcClient.Width(), rcClient.Height(), hMemDC, 0, 0,
						SRCCOPY);

		::SelectPalette(hMemDC, hOldMemPalette, FALSE);
		::SelectPalette(hSrcDC, hOldPalette, FALSE);

		::SelectObject(hMemDC, hbmOld);
		::DeleteObject(hbmMem);
 
		::DeleteDC(hMemDC);
	}
}

void CToolbarButton::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CToolbarButton::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}

void CToolbarButton::OnCaptureChanged( CWnd* pWnd )
{
}
 
UINT CToolbarButton::OnGetDlgCode(void )
{
	return DLGC_WANTALLKEYS;

}

LRESULT CToolbarButton::OnToolTipNeedText(WPARAM wParam, LPARAM lParam)
{
	LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT) lParam;
	GetParentFrame()->SendMessage(TB_FILLINTOOLTIP, WPARAM(m_hWnd), lParam);

	return 1;
}

void CToolbarButton::OnTimer( UINT  nIDEvent )
{
	if(nIDEvent == IDT_MENU)
	{
		KillTimer( IDT_MENU );

		SetState(eBUTTON_DOWN);

		RedrawWindow();


		if(m_dwButtonStyle & TB_HAS_DRAGABLE_MENU)
		{
			CPoint point = RequestMenuPlacement();

			if(m_pDropMenu == NULL || !m_pDropMenu->IsOpen())
			{
				int nCount;
				if(m_pDropMenu != NULL)
				{
					nCount = m_pDropMenu->GetMenuItemCount();

					// clean out the menu before adding to it
					for(int i = nCount - 1; i >= 0; i--)
					{
						m_pDropMenu->DeleteMenu(i, MF_BYPOSITION);
					}
					m_pDropMenu->DestroyDropMenu();
					delete m_pDropMenu;
				}

				m_pDropMenu = new CDropMenu;

				SendMessage(NSDRAGMENUOPEN, (WPARAM)GetButtonCommand(), (LPARAM)m_pDropMenu);

				nCount = m_pDropMenu->GetMenuItemCount();

				if(nCount > 0)
				{
					CDropMenuDropTarget *dropTarget = new CDropMenuDropTarget(m_pDropMenu);

					m_pDropMenu->TrackDropMenu(this, point.x, point.y, m_bDraggingOnButton, dropTarget, m_bDraggingOnButton);
				}
			}
			
		}
		else
		{

			CPoint point(0, GetButtonSize().cy);
			ClientToScreen(&point);

			CMenu &pMenu = GetButtonMenu();

#ifdef _WIN32
			WFE_GetOwnerFrame(this)->SendMessage(NSBUTTONMENUOPEN, MAKEWPARAM(GetButtonCommand(), 0 ),(LPARAM)pMenu.m_hMenu);
#else
			WFE_GetOwnerFrame(this)->SendMessage(NSBUTTONMENUOPEN, (WPARAM) GetButtonCommand(), MAKELPARAM(pMenu.m_hMenu, 0));
#endif
			SetMenuShowing(TRUE);

			CWnd *pWnd = WFE_GetOwnerFrame(this);
			CRect rect;
			GetWindowRect(&rect);

			pMenu.TrackPopupMenu(TPM_LEFTALIGN,point.x, point.y, pWnd ? pWnd : this, &rect);
				
			SetMenuShowing(FALSE);

			int nCount = pMenu.GetMenuItemCount();

			for(int i = 0; i < nCount ; i++)
			{
				pMenu.DeleteMenu(0, MF_BYPOSITION);
			}

		}

		SetState( eNORMAL);
		RedrawWindow();
	}
	if(nIDEvent == IDT_BUTTONFOCUS)
	{
		RemoveButtonFocus();
	}


}

LRESULT CToolbarButton::OnDragMenuOpen(WPARAM wParam, LPARAM lParam)
{
	GetParentFrame()->SendMessage(NSDRAGMENUOPEN,wParam, lParam);

	return 1;
}

LRESULT CToolbarButton::OnDraggingOnButton(WPARAM wParam, LPARAM lParam)
{

	m_bDraggingOnButton = TRUE;
	if(m_dwButtonStyle & TB_HAS_TIMED_MENU)
	{
		m_hMenuTimer = SetTimer(IDT_MENU, MENU_DELAY_MS, NULL);
	}
	else if(m_dwButtonStyle & TB_HAS_IMMEDIATE_MENU)
	{
		m_hMenuTimer = SetTimer(IDT_MENU, 0, NULL);
	}

	return 1;
}

void CToolbarButton::OnPaletteChanged( CWnd* pFocusWnd )
{
	if (pFocusWnd != this ) {
		Invalidate();
		UpdateWindow();
	}

}

void CToolbarButton::OnSysColorChange( )
{
	if(!m_bBitmapFromParent)
	{
		if(m_bIsResourceID)
		{
			if(m_hBmpImg)
				::DeleteObject(m_hBmpImg);

			HINSTANCE hInst = AfxGetResourceHandle();
			HDC hDC = ::GetDC(m_hWnd);

			HPALETTE hPalette= WFE_GetUIPalette(GetParentFrame());
			m_hBmpImg = WFE_LoadTransparentBitmap(hInst, hDC, sysInfo.m_clrBtnFace, RGB(255, 0, 255), hPalette, m_nBitmapID);
			
			::ReleaseDC(m_hWnd, hDC);
			Invalidate();
		}
	}
}

HBRUSH CToolbarButton::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	if(pWnd == m_pTextEdit && nCtlColor == CTLCOLOR_EDIT)
	{

		pDC->SetBkColor(sysInfo.m_clrBtnFace);
		pDC->SetTextColor(sysInfo.m_clrBtnText);
		return sysInfo.m_hbrBtnFace;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//					Helper Functions for CToolbarButton
//////////////////////////////////////////////////////////////////////////

void CToolbarButton::DrawPicturesAndTextMode(HDC hDC, CRect rect)
{

	DrawBitmapOnTop(hDC, rect);
}

void CToolbarButton::DrawPicturesMode(HDC hDC, CRect rect)
{

	DrawButtonBitmap(hDC, rect);

}

void CToolbarButton::DrawTextMode(HDC hDC, CRect rect)
{

	DrawTextOnly(hDC, rect);
}

void CToolbarButton::DrawBitmapOnTop(HDC hDC, CRect rect)
{
	CRect bitmapRect = rect;

	//goes 2 pixels from top
	bitmapRect.top += BITMAPVERTMARGIN;
	// Adjust the image rect
	bitmapRect.bottom = bitmapRect.top + m_bitmapSize.cy;
	
	DrawButtonBitmap(hDC, bitmapRect);

		// must make room for text . Note that our owner is
	// responsible for making sure our overall height is big enough to
	// hold both text and graphics.
	CString strTxt(m_pButtonText);

	if(m_nMaxTextChars != SHOW_ALL_CHARACTERS)
	{
		 strTxt = strTxt.Left(m_nMaxTextChars);
	}

	HFONT font = WFE_GetUIFont(hDC);

	HFONT hOldFont = (HFONT)::SelectObject(hDC, font);

	SIZE sizeTxt;

	CRect sizeRect(0,0,150,0);
	::DrawText(hDC, strTxt, strTxt.GetLength(), &sizeRect, DT_CALCRECT | DT_WORDBREAK);
	sizeTxt.cx = sizeRect.Width();
	sizeTxt.cy = sizeRect.Height();

	// text's baseline should be 3 pixels from bottom
	CRect txtRect = rect;

	txtRect.top = bitmapRect.bottom +TEXT_BITMAPVERTMARGIN;
	
	DrawButtonText(hDC, txtRect, sizeTxt, strTxt);

	::SelectObject(hDC, hOldFont);

}

void CToolbarButton::DrawBitmapOnSide(HDC hDC, CRect rect)
{

	CRect bitmapRect = rect;

	bitmapRect.right = m_bitmapSize.cx + BORDERSIZE;

	DrawButtonBitmap(hDC, bitmapRect);

	CString strTxt(m_pButtonText);

	if(m_nMaxTextChars != SHOW_ALL_CHARACTERS)
	{
		 strTxt = strTxt.Left(m_nMaxTextChars);
	}

	HFONT font = WFE_GetUIFont(hDC);

	HFONT hOldFont = (HFONT)::SelectObject(hDC, font);

	SIZE sizeTxt;
#if defined (WIN32)
	::GetTextExtentPoint32(hDC, (LPCSTR)strTxt, strTxt.GetLength(), &sizeTxt);
#else
	DWORD dwSize = ::GetTextExtent(hDC, (LPCSTR)strTxt, strTxt.GetLength());
	sizeTxt.cx = LOWORD(dwSize);
	sizeTxt.cy = HIWORD(dwSize);
#endif

	CRect txtRect = rect;

	txtRect.left = bitmapRect.right;

	DrawButtonText(hDC, txtRect, sizeTxt, strTxt);

	::SelectObject(hDC, hOldFont);
}

void CToolbarButton::DrawTextOnly(HDC hDC, CRect rect)
{

	CString strTxt(m_pButtonText);

	if(m_nMaxTextChars != SHOW_ALL_CHARACTERS)
	{
		 strTxt = strTxt.Left(m_nMaxTextChars);
	}

	HFONT font = WFE_GetUIFont(hDC);

	HFONT hOldFont = (HFONT)::SelectObject(hDC, font);


	SIZE sizeTxt;

	CRect sizeRect(0,0,150,0);
	::DrawText(hDC, strTxt, strTxt.GetLength(), &sizeRect, DT_CALCRECT | DT_WORDBREAK);
	sizeTxt.cx = sizeRect.Width();
	sizeTxt.cy = sizeRect.Height();

	CRect txtRect = rect;

	DrawButtonText(hDC, txtRect, sizeTxt, strTxt);


	::SelectObject(hDC, hOldFont);
}

void CToolbarButton::DrawButtonText(HDC hDC, CRect rcTxt, CSize sizeTxt, CString strTxt)
{

	int nOldBkMode = ::SetBkMode(hDC, TRANSPARENT);


	if(m_bEnabled)
	{
		UINT nFormat = strTxt.Find('\n') != -1 ? 0 : DT_SINGLELINE | DT_VCENTER;

		COLORREF oldColor;

		if (hasCustomTextColor)
		{
			oldColor = ::SetTextColor(hDC, customTextColor);
		}
		else if(m_eState == eNORMAL || m_eState == eBUTTON_CHECKED)
		{
			oldColor = ::SetTextColor(hDC, sysInfo.m_clrBtnText);
			if (m_bDepressed)
				oldColor = ::SetTextColor(hDC, RGB(255,255,255));
		}
		else if (m_eState == eBUTTON_DOWN)
		{
			oldColor = ::SetTextColor(hDC, RGB(0, 0, 128));
		}
		else if(m_eState == eBUTTON_UP)
		{
			oldColor = ::SetTextColor(hDC, RGB(0, 0, 255));
		}

		DrawText(hDC, (LPCSTR)strTxt, -1, &rcTxt, DT_CENTER | DT_EXTERNALLEADING | nFormat);

		::SetTextColor(hDC, oldColor);
	}
	else
	{
		CRect textRect = rcTxt;

		
		textRect.left = textRect.left + (textRect.Width() - sizeTxt.cx) / 2;
		textRect.top = textRect.top + (textRect.Height() - sizeTxt.cy) / 2;

		CRect textBackgroundRect = textRect;

#ifdef XP_WIN32
		if(sysInfo.m_bWin4)
		{
			
			TEXTMETRIC tm;
			
			GetTextMetrics(hDC, &tm);

			int descent = tm.tmDescent;
			int retVal = DrawState(hDC, NULL, NULL, (LPARAM)strTxt.LockBuffer(), 0, textRect.left, textRect.top, 0, 0, DST_TEXT | DSS_DISABLED);
			strTxt.UnlockBuffer();

		}
		else
#endif
		{
			UINT nFormat = strTxt.Find('\n') != -1 ? 0 : DT_SINGLELINE;

			textBackgroundRect.left +=1;
			textBackgroundRect.top +=1;
			
			COLORREF oldColor = ::SetTextColor(hDC,  GetSysColor(COLOR_BTNHIGHLIGHT )); // white
			::DrawText( hDC, (LPCSTR) strTxt, -1, &textBackgroundRect,   DT_EXTERNALLEADING | nFormat);
			::SetTextColor(hDC, GetSysColor( COLOR_GRAYTEXT ) );
			::DrawText(hDC, (LPCSTR) strTxt, -1, &textRect,   DT_EXTERNALLEADING | nFormat);
			::SetTextColor(hDC, oldColor );
		}
	}

	::SetBkMode(hDC, nOldBkMode);
}


void CToolbarButton::DrawButtonBitmap(HDC hDC, CRect rcImg)
{
	if(m_hBmpImg != NULL)
	{
		// Create a scratch DC and select our bitmap into it.
		HDC pBmpDC  = ::CreateCompatibleDC(hDC);
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());

		CBitmap BmpImg;

		CPoint ptDst;

		HINSTANCE hInst = AfxGetResourceHandle();


		HBITMAP hBmpImg;

		hBmpImg = m_hBmpImg;

		HBITMAP hOldBmp = (HBITMAP)::SelectObject(pBmpDC, hBmpImg);
		HPALETTE hOldPal = ::SelectPalette(pBmpDC, WFE_GetUIPalette(NULL), TRUE);
		::RealizePalette(pBmpDC);
		// Get the image dimensions
		CSize sizeImg;
		BITMAP bmp;

		::GetObject(hBmpImg, sizeof(bmp), &bmp);
		sizeImg.cx = bmp.bmWidth;
		sizeImg.cy = bmp.bmHeight;

		// Center the image within the button	
		ptDst.x = (rcImg.Width() >= m_bitmapSize.cx) ?
			rcImg.left + (((rcImg.Width() - m_bitmapSize.cx) + 1) / 2) : 0;
		
		ptDst.y = rcImg.top; 
		// If we're in the checked state, shift the image one pixel
		if (m_eState == eBUTTON_CHECKED || (m_eState == eBUTTON_UP && m_nChecked == 1))
		{
			ptDst.x += 1;
			ptDst.y += 1;
		}
	
		// Call the handy transparent blit function to paint the bitmap over
		// whatever colors exist.
		
		CPoint bitmapStart;

		BTN_STATE eState = m_eState;

		if(m_eState == eBUTTON_CHECKED)
			// A checked button has same bitmap as the normal state with no mouse-over
			eState = eNORMAL;
		else if(m_eState == eBUTTON_UP && m_nChecked == 2)
			// if we are in the mouse over mode, but indeterminate we want our bitmap to have a disabled look
			eState = eDISABLED;

        if(m_bIsResourceID)
			bitmapStart = CPoint(m_nBitmapIndex * m_bitmapSize.cx, m_bEnabled ? m_bitmapSize.cy * eState : m_bitmapSize.cy);

		if(m_bIsResourceID)  
		{
			UpdateIconInfo();
			if(m_nIconType == LOCAL_FILE)
			{
				DrawLocalIcon(hDC, ptDst.x, ptDst.y);
			}
			else if (m_nIconType == ARBITRARY_URL)
			{
				DrawCustomIcon(hDC, ptDst.x, ptDst.y);
			}
			else       
                ::BitBlt(hDC, ptDst.x, ptDst.y, m_bitmapSize.cx, m_bitmapSize.cy, 
					 pBmpDC, bitmapStart.x, bitmapStart.y, SRCCOPY);
		}
		else
		{
			CSize destSize;

			if(sizeImg.cx > sizeImg.cy)
			{
				destSize.cx = m_bitmapSize.cx;
				destSize.cy = (int)(m_bitmapSize.cy * ((double)sizeImg.cy / sizeImg.cx));
			}
			else
			{
				destSize.cx = (int)(m_bitmapSize.cx * ((double)sizeImg.cx/ sizeImg.cy));
				destSize.cy = m_bitmapSize.cy;
			}
			StretchBlt(hDC, ptDst.x, ptDst.y, destSize.cx, destSize.cy,
						pBmpDC, 0, 0, sizeImg.cx, sizeImg.cy, SRCCOPY);
		}

		// Cleanup

		::SelectObject(pBmpDC, hOldBmp);
		::SelectPalette(pBmpDC, hOldPal, TRUE);
		::DeleteDC(pBmpDC);

	}
}

/****************************************************************************
*
*	Function: DrawUpButton
*
*	PARAMETERS:
*		pDC		- pointer to DC to draw on
*		rect	- client rect to draw in
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for drawing the 3d "up button" look.
*		This is a static function so it may be called without instantiating
*		the object.
*
****************************************************************************/
void CToolbarButton::DrawUpButton(HDC hDC, CRect & rect)
{
	HBRUSH br = ::CreateSolidBrush(::GetSysColor(COLOR_BTNHIGHLIGHT));

	CRect rc(rect.left+1, rect.top+1, rect.right, 2);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.left+1, rect.top+1, rect.left+2, rect.bottom - 1);
	::FillRect(hDC, rc, br);
	::DeleteObject(br);
	
	br = ::CreateSolidBrush((RGB(0, 0, 0)));
    // Extra focus indication is solid black rect around outside
    // Looks more like "Win3.0" style buttons
	FrameRect(hDC, &rect, br );

	// Hilight
	// Shadow
	::DeleteObject(br);
	br = ::CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));
	rc.SetRect(rect.left+1, rect.bottom - 2, rect.right - 1 , rect.bottom - 1);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.right - 2, rect.top, rect.right - 1, rect.bottom - 1);
	::FillRect(hDC, rc, br);
	::DeleteObject(br);
} 
/****************************************************************************
*
*	FUNCTION:  DrawDownButton
*
*	PARAMETERS:
*		pDC		- pointer to DC to draw on
*		rect	- client rect to draw in
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for drawing the 3d "down button" look.
*		This is a static function so it may be called without instantiating
*		the object.
*
****************************************************************************/

void CToolbarButton::DrawDownButton(HDC hDC, CRect & rect)
{
    DrawCheckedButton(hDC, rect);
	HBRUSH br = ::CreateSolidBrush((RGB(0, 0, 0)));

	::FrameRect(hDC, &rect, br );
	::DeleteObject(br);
}

/****************************************************************************
*
*	FUNCTION:  DrawCheckedButton
*
*	PARAMETERS:
*		pDC		- pointer to DC to draw on
*		rect	- client rect to draw in
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for drawing the 3d "down button" look.
*		This is a static function so it may be called without instantiating
*		the object.
*
****************************************************************************/

void CToolbarButton::DrawCheckedButton(HDC hDC, CRect & rect)
{
	// Hilight
	CRect rc(rect.left+1, rect.bottom - 2, rect.right - 1, rect.bottom - 1);
	HBRUSH br = ::CreateSolidBrush(::GetSysColor(COLOR_BTNHIGHLIGHT));
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.right - 2, rect.top+1, rect.right - 1, rect.bottom - 1);
	::FillRect(hDC, rc, br);
	
	// Shadow
	::DeleteObject(br);
	br = ::CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));
	rc.SetRect(rect.left+1, rect.top+1, rect.right - 1, 2);
	::FillRect(hDC, rc, br);
	rc.SetRect(rect.left+1, rect.top+1, rect.left+2, rect.bottom - 1);
	::FillRect(hDC, rc, br);
	::DeleteObject(br);
}

void CALLBACK EXPORT NSButtonMenuTimerProc(
    HWND hwnd,	// handle of window for timer messages 
    UINT uMsg,	// WM_TIMER message
    UINT idEvent,	// timer identifier
    ULONG dwTime 	// current system time
    )
{
}

CSize CToolbarButton::GetBitmapOnTopSize(CString strTxt, int c)
{

	HDC hDC = ::GetDC(m_hWnd);

	if (c == 0)
	{
		strTxt = "";
	}
	else if(c != SHOW_ALL_CHARACTERS)
	{
         if (strTxt.GetLength() > c)
             strTxt = strTxt.Left(c - 3) + "...";
         else strTxt = strTxt.Left(c);
	}

	HFONT font = WFE_GetUIFont(hDC);

	HFONT hOldFont = (HFONT)::SelectObject(hDC, font);
	
	CRect textRect(0,0,150,0);

	CSize sizeTxt;

	if(strTxt.IsEmpty())
		sizeTxt.cx = sizeTxt.cy = 0;
	else
	{
		DrawText(hDC, strTxt, strTxt.GetLength(), &textRect, DT_CALCRECT | DT_WORDBREAK);
		sizeTxt.cx = textRect.Width();
		sizeTxt.cy = textRect.Height();
	}

	int nWidth, nHeight;

	::SelectObject(hDC, hOldFont);

	nWidth = ((m_bitmapSize.cx > sizeTxt.cx) ? m_bitmapSize.cx : sizeTxt.cx) + (2 * BORDERSIZE) + HORIZMARGINSIZE;
	nHeight = m_bitmapSize.cy + sizeTxt.cy + TEXTVERTMARGIN + TEXT_BITMAPVERTMARGIN + BITMAPVERTMARGIN + (2 *BORDERSIZE);

	::ReleaseDC(m_hWnd, hDC);


	return CSize(nWidth, nHeight);
}

CSize CToolbarButton::GetBitmapOnlySize(void)
{

	int nWidth, nHeight;


	nWidth = m_bitmapSize.cx + (2 * BORDERSIZE) + HORIZMARGINSIZE;
	nHeight = m_bitmapSize.cy + (2 * BORDERSIZE);

	return CSize(nWidth, nHeight);

}

CSize CToolbarButton::GetBitmapOnSideSize(CString strTxt, int c)
{

	HDC hDC = ::GetDC(m_hWnd);

	if(c != SHOW_ALL_CHARACTERS)
	{
         if (strTxt.GetLength() > c)
             strTxt = strTxt.Left(c - 3) + "...";
         else strTxt = strTxt.Left(c);
	}

	HFONT font = WFE_GetUIFont(hDC);

	HFONT hOldFont = (HFONT)::SelectObject(hDC, font);
	
	SIZE sizeTxt;
#if defined (_WIN32)
	::GetTextExtentPoint32(hDC, (LPCSTR)strTxt, strTxt.GetLength(), &sizeTxt);
#else
	DWORD dwSize = ::GetTextExtent(hDC, (LPCSTR)strTxt, strTxt.GetLength());
	sizeTxt.cx = LOWORD(dwSize);
	sizeTxt.cy = HIWORD(dwSize);
#endif

	::SelectObject(hDC, hOldFont);

	int nWidth, nHeight;

	nWidth = sizeTxt.cx + m_bitmapSize.cx + (2 * BORDERSIZE) + HORIZMARGINSIZE;
	nHeight =((m_bitmapSize.cy > sizeTxt.cy) ? m_bitmapSize.cy : sizeTxt.cy) + (2 * BORDERSIZE);

	::ReleaseDC(m_hWnd, hDC);
	return CSize(nWidth, nHeight);
}

CSize CToolbarButton::GetTextOnlySize(CString strTxt, int c)
{

	HDC hDC = ::GetDC(m_hWnd);

	if(c != SHOW_ALL_CHARACTERS)
	{
         if (strTxt.GetLength() > c)
             strTxt = strTxt.Left(c - 3) + "...";
         else strTxt = strTxt.Left(c);
	}

	HFONT font = WFE_GetUIFont(hDC);

	HFONT hOldFont = (HFONT)::SelectObject(hDC, font);
	
	CRect textRect(0,0,150,0);

	CSize sizeTxt;

	if(strTxt.IsEmpty())
		sizeTxt.cx = sizeTxt.cy = 0;
	else
	{
		DrawText(hDC, strTxt, strTxt.GetLength(), &textRect, DT_CALCRECT | DT_WORDBREAK);

		sizeTxt.cx = textRect.Width();
		sizeTxt.cy = textRect.Height();
	}

	int nWidth, nHeight;

	::SelectObject(hDC, hOldFont);

	nWidth = sizeTxt.cx +  (2 * BORDERSIZE) + HORIZMARGINSIZE;
	nHeight = sizeTxt.cy + (2 * BORDERSIZE) + (2 * TEXTONLYVERTMARGIN);

	::ReleaseDC(m_hWnd, hDC);

	return CSize(nWidth, nHeight);

}

 void CToolbarButton::OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler)
{
	m_state.m_pOther = this;

	m_state.m_nID = m_nCommand;

	m_state.DoUpdate(pTarget, bDisableIfNoHndler);
}

 void CToolbarButton::RemoveButtonFocus(void)
 {

	BOOL bStatusText = (strcmp(m_pStatusText, "") != 0);
	POINT point;

	KillTimer(IDT_BUTTONFOCUS);
	m_hFocusTimer = 0;
	GetCursorPos(&point);

	CRect rcClient;
	GetWindowRect(&rcClient);

	if (!rcClient.PtInRect(point))
	{
		m_bHaveFocus = FALSE;

		if ( bStatusText && WFE_GetOwnerFrame(this) != NULL) {
			WFE_GetOwnerFrame(this)->SendMessage( WM_SETMESSAGESTRING,
										   (WPARAM) 0, (LPARAM) "" );
		}

	
		//if we lose capture and we don't have a menu showing we just want to show
		//the normal state.  If we have a menu showing we want to show the button down
		//state
		if(m_nChecked == 1)
		{
			m_eState = eBUTTON_CHECKED;
			RedrawWindow();
		}
		else if(m_nChecked == 2)
		{
			m_eState = eDISABLED;
			RedrawWindow();
		}
		else if(m_eState != eNORMAL && !m_bMenuShowing)
		{
			if(m_nChecked == 0)
			{
				m_eState = eNORMAL;
				RedrawWindow();
			}
		}
		else if(m_bMenuShowing)
		{
			m_eState = eBUTTON_CHECKED; //eBUTTON_DOWN;
			RedrawWindow();
		}
		else if(m_bEnabled)
		{
			if(m_nChecked == 0)
			{
				m_eState = eNORMAL;
				RedrawWindow();
			}
		}
		UpdateWindow();
		pCurrentButton = NULL;
	}
	else
		m_hFocusTimer = SetTimer(IDT_BUTTONFOCUS, BUTTONFOCUS_DELAY_MS, NULL);
 }

 BOOL CToolbarButton::CreateRightMouseMenu(void)
 {
 	if((m_dwButtonStyle & TB_HAS_TIMED_MENU) && m_bEnabled)
	{

		#ifdef _WIN32
			GetParentFrame()->SendMessage(NSBUTTONMENUOPEN, MAKEWPARAM(m_nCommand, 0 ),(LPARAM)m_menu.m_hMenu);
		#else
			GetParentFrame()->SendMessage(NSBUTTONMENUOPEN, (WPARAM)m_nCommand, MAKELPARAM(m_menu.m_hMenu,0));
		#endif
		return TRUE;
	}

	return FALSE;
 }

 void CToolbarButton::DisplayAndTrackMenu(void)
 {

	m_eState = eBUTTON_DOWN;
	RedrawWindow();

	CPoint point = RequestMenuPlacement();
	
	CWnd *pWnd = GetMenuParent();
	CRect rect;
	GetWindowRect(&rect);
	KillTimer(IDT_BUTTONFOCUS);
	m_hFocusTimer = 0;

	m_menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON,point.x, point.y, pWnd ? pWnd : this, &rect);
	
	int nCount = m_menu.GetMenuItemCount();

	for(int i = 0; i < nCount ; i++)
	{
		m_menu.DeleteMenu(0, MF_BYPOSITION);
	}

	if(m_nChecked == 0)
		m_eState = eNORMAL;
	RedrawWindow();

 }

 CWnd* CToolbarButton::GetMenuParent(void)
 {
	return GetParentFrame();
 }

 void CToolbarButton::GetTextRect(CRect &rect)
 {
	GetClientRect(rect);

	rect.InflateRect(-BORDERSIZE, -BORDERSIZE);


	if(m_nToolbarStyle == TB_PICTURESANDTEXT)
	{
		GetPicturesAndTextModeTextRect(rect);
	}
	else if(m_nToolbarStyle == TB_PICTURES)
	{
		GetPicturesModeTextRect(rect);
	}
	else
	{
		GetTextModeTextRect(rect);
	}


 }

 void CToolbarButton::GetPicturesAndTextModeTextRect(CRect &rect)
 {
	GetBitmapOnTopTextRect(rect);
 }

 void CToolbarButton::GetPicturesModeTextRect(CRect &rect)
 {
	// there is no text
	rect.SetRect(0,0,0,0);

 }


 void CToolbarButton::GetTextModeTextRect(CRect &rect)
 {
	GetTextOnlyTextRect(rect);
 }

 void CToolbarButton::GetBitmapOnTopTextRect(CRect &rect)
 {
	CRect bitmapRect = rect;

	//goes 2 pixels from top
	bitmapRect.top += BITMAPVERTMARGIN;
	// Adjust the image rect
	bitmapRect.bottom = bitmapRect.top + m_bitmapSize.cy;
	
	rect.top = bitmapRect.bottom +TEXT_BITMAPVERTMARGIN;

 }

 void CToolbarButton::GetTextOnlyTextRect(CRect &rect)
 {
		
 }

 void CToolbarButton::GetBitmapOnSideTextRect(CRect &rect)
 {
	CRect bitmapRect = rect;

	bitmapRect.right = m_bitmapSize.cx + BORDERSIZE;

	rect.left = bitmapRect.right;

 }

// End CToolbar Implementation

//////////////////////////////////////////////////////////////////////////////////////
//							CDragableToolbarButton
//////////////////////////////////////////////////////////////////////////////////////

CDragableToolbarButton::CDragableToolbarButton()
{
	m_bDragging = FALSE;
}

//////////////////////////////////////////////////////////////////////////
//					Messages for CDragableToolbarButton
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CDragableToolbarButton, CToolbarButton)
	//{{AFX_MSG_MAP(CWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

void CDragableToolbarButton::OnLButtonUp(UINT nFlags, CPoint point)
{
	CToolbarButton::OnLButtonUp(nFlags, point);

	if(m_bDragging)
	{
		ReleaseCapture();
		m_bDragging = FALSE;
	}
}

void CDragableToolbarButton::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_bDragging = TRUE;

	m_draggingPoint = point;

	CToolbarButton::OnLButtonDown(nFlags, point);

}

void CDragableToolbarButton::OnMouseMove(UINT nFlags, CPoint point)
{
	if(m_bDragging)
	{
		if((abs(point.x - m_draggingPoint.x) > 5)
             || (abs(point.y - m_draggingPoint.y) > 5))
		{
			ButtonDragged();
			m_bDragging = FALSE;

			GetParent()->PostMessage(NSBUTTONDRAGGING, 0,(LPARAM)m_hWnd);
			return;
		}
	}
	CToolbarButton::OnMouseMove(nFlags, point);
}

void CDragableToolbarButton::OnTimer( UINT  nIDEvent )
{
	CToolbarButton::OnTimer(nIDEvent);

	if(nIDEvent == IDT_MENU)
	{
		m_bDragging = FALSE;
	}
}
// End CDragableToolbarButton Implementation

//////////////////////////////////////////////////////////////////////////////////////
//							CStationaryToolbarButton
//////////////////////////////////////////////////////////////////////////////////////
int CStationaryToolbarButton::Create(CWnd *pParent, int nToolbarStyle, 
								CSize noviceButtonSize, CSize advancedButtonSize,
								LPCTSTR pButtonText, LPCTSTR pToolTipText, 
								LPCTSTR pStatusText, UINT nBitmapID, UINT nBitmapIndex,
								CSize bitmapSize, BOOL bNeedsUpdate, UINT nCommand, 
								int nMaxTextChars, DWORD dwButtonStyle, int nMinTextChars)
{
	
	return(CToolbarButton::Create(pParent, nToolbarStyle, noviceButtonSize, advancedButtonSize,
						   pButtonText, pToolTipText, pStatusText, nBitmapID, nBitmapIndex, 
						   bitmapSize, bNeedsUpdate,
						   nCommand, nMaxTextChars, nMinTextChars, dwButtonStyle));


}

int CStationaryToolbarButton::Create(CWnd *pParent, int nToolbarStyle, 
									CSize noviceButtonSize, CSize advancedButtonSize,
									LPCTSTR pButtonText, LPCTSTR pToolTipText, 
									LPCTSTR pStatusText, LPCTSTR pBitmapFile,
									CSize bitmapSize, BOOL bNeedsUpdate, UINT nCommand, 
									int nMaxTextChars, DWORD dwButtonStyle, int nMinTextChars)
{
	return(CToolbarButton::Create(pParent, nToolbarStyle, noviceButtonSize, advancedButtonSize,
						   pButtonText, pToolTipText, pStatusText, pBitmapFile, 
						   bitmapSize, bNeedsUpdate,
						   nCommand, nMaxTextChars, nMinTextChars, dwButtonStyle));

}


//////////////////////////////////////////////////////////////////////////
//					Messages for CStationaryToolbarButton
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CStationaryToolbarButton, CToolbarButton)
	//{{AFX_MSG_MAP(CWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

void CStationaryToolbarButton::OnLButtonUp(UINT nFlags, CPoint point)
{
	CToolbarButton::OnLButtonUp(nFlags, point);
}

void CStationaryToolbarButton::OnLButtonDown(UINT nFlags, CPoint point)
{
	CToolbarButton::OnLButtonDown(nFlags, point);
}

void CStationaryToolbarButton::OnMouseMove(UINT nFlags, CPoint point)
{
	CToolbarButton::OnMouseMove(nFlags, point);
}
// End CStationaryToolbar Implementation





 //////////////////////////////////////////////////////////////////////////////////////
//							CCommandToolbarButton
//////////////////////////////////////////////////////////////////////////////////////

CCommandToolbarButton::CCommandToolbarButton()
{
	m_pActionOwner = NULL;
}

int CCommandToolbarButton::Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
								LPCTSTR pButtonText, LPCTSTR pToolTipText, 
								LPCTSTR pStatusText, UINT nBitmapID, UINT nBitmapIndex,
								CSize bitmapSize, UINT nCommand, int nMaxTextChars, DWORD dwButtonStyle, int nMinTextChars)
{
	
	return(CStationaryToolbarButton::Create(pParent, nToolbarStyle, noviceButtonSize, advancedButtonSize,
						   pButtonText, pToolTipText, pStatusText, nBitmapID, nBitmapIndex, bitmapSize, TRUE,
						   nCommand, nMaxTextChars, dwButtonStyle, nMinTextChars));


}

int CCommandToolbarButton::Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
								  LPCTSTR pButtonText, LPCTSTR pToolTipText, 
								  LPCTSTR pStatusText, LPCTSTR pBitmapFile,
								  CSize bitmapSize, UINT nCommand, int nMaxTextChars, DWORD dwButtonStyle, int nMinTextChars)
{
	return(CStationaryToolbarButton::Create(pParent, nToolbarStyle, noviceButtonSize, advancedButtonSize,
						   pButtonText, pToolTipText, pStatusText, pBitmapFile, bitmapSize, TRUE,
						   nCommand, nMaxTextChars, dwButtonStyle, nMinTextChars));

}

void CCommandToolbarButton::OnAction(void)
{
	 CWnd *pActionOwner;

	 if(m_pActionOwner == NULL)
		pActionOwner = WFE_GetOwnerFrame(this);
	 else
		pActionOwner = m_pActionOwner;

#ifdef _WIN32
	pActionOwner->PostMessage(WM_COMMAND, MAKEWPARAM(m_nCommand, m_nCommand), 0);
#else
	pActionOwner->PostMessage(WM_COMMAND, (WPARAM) m_nCommand, MAKELPARAM(this->m_hWnd, 0));
#endif
}

 void CCommandToolbarButton::FillInOleDataSource(COleDataSource *pDataSource)
 {

	HGLOBAL hCommandButton = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(COMMANDBUTTONITEM));
    if(!hCommandButton) {
        return;
    }

    LPCOMMANDBUTTONITEM pCommandButton = (LPCOMMANDBUTTONITEM)GlobalLock(hCommandButton);
	
	CToolbarButton::FillInButtonData(&(pCommandButton->buttonInfo));

    GlobalUnlock(hCommandButton);

    // Create the DataSourceObject
    pDataSource->CacheGlobalData(RegisterClipboardFormat(NETSCAPE_COMMAND_BUTTON_FORMAT), hCommandButton);
  

 }

 void CCommandToolbarButton::SetActionMessageOwner(CWnd *pActionOwner)
 {
	 m_pActionOwner = pActionOwner;
 }
