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

// implements the CNSToolbar2 class

#include "stdafx.h"
#include "toolbar2.h"

#define SPACE_BETWEEN_BUTTONS 2
#define LEFT_TOOLBAR_MARGIN 10


//IMPLEMENT_DYNCREATE(CNSToolbar2, CWnd)


SCODE CToolbarDropSource::GiveFeedback(DROPEFFECT dropEffect)
{
  if(dropEffect == DROPEFFECT_MOVE)
  {
	   SetCursor(theApp.LoadCursor( IDC_MOVEBUTTON));
#ifdef WIN32
	   return NO_ERROR;
#else
	   return NOERROR;
#endif

  }
	 return COleDropSource::GiveFeedback(dropEffect);
 }



// Function - CNSToolbar2
//
// Constructor
// 
// Arguments - nMaxButtons		the maximum # of buttons we can manage
//			   bNoviceMode	    whether or not we are in Novice Mode
//			   nNoviceSize		our height in Novice Mode
//			   nAdvancedSize	our height in Advanced Mode
//
// Returns - Nothing
CNSToolbar2::CNSToolbar2(int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, int nPicturesHeight,
						 int nTextHeight)
{

	m_nMaxButtons = nMaxButtons;
	m_nToolbarStyle = nToolbarStyle;
	m_nPicturesAndTextHeight = nPicturesAndTextHeight;
	m_nPicturesHeight = nPicturesHeight;
	m_nTextHeight = nTextHeight;

	m_pButtonArray = new CToolbarButton*[nMaxButtons];
	m_nNumButtons = 0;

	m_bDragging = FALSE;
	m_pCustToolbar = NULL;
	m_nMaxButtonWidth = 0;
	m_nMaxButtonHeight = 0;

	m_nHeight = GetHeight();
	m_nWidth = 0;
	m_hBitmap = NULL;
	m_nBitmapID = 0;
	m_bButtonsSameWidth = TRUE;

}

CNSToolbar2::~CNSToolbar2()
{
	if(m_hBitmap != NULL)
		::DeleteObject(m_hBitmap);

	int i;

	for( i = 0; i < m_nNumButtons; i++)
		delete m_pButtonArray[i];

	int nCount = m_pHiddenButtons.GetSize();

	for(i = 0; i < nCount; i++)
	{
		CToolbarButton * pButton = (CToolbarButton*)m_pHiddenButtons[i];
		delete pButton;
	}

	m_pHiddenButtons.RemoveAll();

	delete [] m_pButtonArray;
}


// Function - Create
//
// Creates a CNSToolbar2
// 
// Arguments - pParent			the parent window
//
// Returns - Non-zero if successful, 0 if failure
int CNSToolbar2::Create(CWnd *pParent)
{
	CRect rect;

	rect.SetRectEmpty();
	CBrush brush;

	DWORD shouldClipChildren = 0;
	if (ShouldClipChildren())
		shouldClipChildren = WS_CLIPCHILDREN;
	
	BOOL bResult = CWnd::Create( theApp.NSToolBarClass, NULL, 
			 WS_CHILD | shouldClipChildren | WS_CLIPSIBLINGS,
			 rect, pParent, 0, NULL);

	if(bResult)
	{
		HDC hDC = ::GetDC(m_hWnd);
		WFE_InitializeUIPalette(hDC);
		::ReleaseDC(m_hWnd, hDC);
	}
	return bResult;
}

// Function - Add
//
// Adds a button to the toolbar in position nIndex and layouts the buttons
// 
// 
// Arguments - pButton			the button to add
//			   nIndex			the index to add it to
//
// Returns - nothing
void CNSToolbar2::AddButtonAtIndex(CToolbarButton *pButton, int nIndex, BOOL bNotify)
{
	// for the moment, don't allow more than m_nMaxButtons.  However, eventually keep track of
	// all items, but just make room for first m_nMaxButtons.
	ASSERT( nIndex >= -1 && nIndex <= m_nNumButtons);


	//if the button is already on the toolbar, remove it
	int nPos;
	if((nPos = FindButton(pButton)) != -1)
	{
		RemoveButton(nPos);
		if(nPos < nIndex)
			nIndex--;
	}

	// if we are going to overflow, just remove the last one.
	if(m_nNumButtons >= m_nMaxButtons && nIndex < m_nMaxButtons)
		delete(RemoveButton(m_nMaxButtons - 1));

	if( nIndex == -1)
	{
		nIndex = m_nNumButtons;
	}

	for(int i = m_nNumButtons; i > nIndex; i--)
	{
		m_pButtonArray[i] = m_pButtonArray[i - 1] ;
		// new button id will have to be set
	}

	m_pButtonArray[i] = pButton;
	if(bNotify)
		SendMessage(CASTUINT(TOOLBAR_BUTTON_ADD), (WPARAM) i, (LPARAM)pButton->m_hWnd);

	m_nNumButtons++;

	if(CheckMaxButtonSizeChanged(pButton, TRUE))
	{
		ChangeButtonSizes();
		LayoutButtons(-1);
		GetParentFrame()->RecalcLayout();
	}
	else
	{
		int nWidth;
		
		if(!m_bButtonsSameWidth)
		{
			CSize size = pButton->GetRequiredButtonSize();
			nWidth = size.cx;
		}
		else
			nWidth = m_nMaxButtonWidth;

		//make sure it's the size of the largest button so far
		pButton->SetButtonSize(CSize(nWidth, m_nMaxButtonHeight));

		LayoutButtons(nIndex - 1);
	}
}

// bAddButton refers to whether or not the button is actually added.
// It might be false if you wish to know what position it would be added to.
int CNSToolbar2::AddButtonAtPoint(CToolbarButton *pButton, CPoint point, BOOL bAddButton)
{

	int nStartX = 0;

	RECT buttonRect;

	for(int i = 0; i< m_nNumButtons; i++)
	{
		m_pButtonArray[i]->GetClientRect(&buttonRect);
		m_pButtonArray[i]->MapWindowPoints(this, &buttonRect);

		nStartX += (buttonRect.right - buttonRect.left) + SPACE_BETWEEN_BUTTONS;
		if(point.x < nStartX) break;

	}

	if(bAddButton)
		AddButtonAtIndex(pButton, i);

	return i;
}

void CNSToolbar2::AddHiddenButton(CToolbarButton *pButton)
{
	m_pHiddenButtons.Add(pButton);

}

// Function - Remove
//
// Removes the button at the given index and redoes the button layout
// 
// Arguments -  nIndex			the index to remove from
//
// Returns - the button being removed
CToolbarButton* CNSToolbar2::RemoveButton(int nIndex, BOOL bNotify)
{
	ASSERT( nIndex < m_nMaxButtons);
	ASSERT( nIndex >=0 && nIndex < m_nNumButtons);

	CToolbarButton *pButton = m_pButtonArray[nIndex];
	if(bNotify)
		SendMessage(CASTUINT(TOOLBAR_BUTTON_REMOVE), (WPARAM)nIndex, (LPARAM)pButton->m_hWnd);

	for(int i = nIndex; i < m_nNumButtons - 1; i++)
	{
		m_pButtonArray[i] = m_pButtonArray[i+1];
		// new button id will have to be set
	}

	m_nNumButtons--;

	pButton->MoveWindow(0,0,0,0);
	
	if(CheckMaxButtonSizeChanged(pButton, FALSE))
	{
		ChangeButtonSizes();
		LayoutButtons(-1);
		GetParentFrame()->RecalcLayout();
	}
	else
	{
		//make sure it's the size of the largest button so far
		LayoutButtons(nIndex - 1);
	}


	return pButton;

}

// Function - Remove
//
// Removes the given button from the toolbar and redoes the button layout
// 
// Arguments -  pButton		the button to remove
//
// Returns - the button being removed or NULL if that button can't be found
CToolbarButton* CNSToolbar2::RemoveButton(CToolbarButton *pButton)
{

	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(m_pButtonArray[i] == pButton)
		{
			return RemoveButton(i);
		}
	}

	return NULL;
}

CToolbarButton* CNSToolbar2::RemoveButtonByCommand(UINT nCommand)
{

	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(m_pButtonArray[i]->GetButtonCommand() == nCommand)
		{
			return RemoveButton(i);
		}
	}

	return NULL;
}

void CNSToolbar2::HideButtonByCommand(UINT nCommand)
{

	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(m_pButtonArray[i]->GetButtonCommand() == nCommand)
		{
			m_pHiddenButtons.Add(RemoveButton(i));
		}
	}

}

void CNSToolbar2::ShowButtonByCommand(UINT nCommand, int nPos)
{

	int nCount = m_pHiddenButtons.GetSize();

	for(int i = 0; i < nCount; i++)
	{
		CToolbarButton *pButton = (CToolbarButton*)m_pHiddenButtons[i];

		if(pButton->GetButtonCommand() == nCommand)
		{
			m_pHiddenButtons.RemoveAt(i, 1);
			AddButton(pButton, nPos);
			return;
		}
	}

}

void CNSToolbar2::RemoveAllButtons(void)
{
	for(int i = m_nNumButtons - 1; i >= 0; i--)
	{
		delete(m_pButtonArray[i]);
	}

	m_nNumButtons = 0;

	m_nMaxButtonWidth = 0;
	m_nMaxButtonHeight = 0;

	m_nWidth = 0;

	RedrawWindow();
}

CToolbarButton *CNSToolbar2::ReplaceButton(UINT nCommand, CToolbarButton *pNewButton)
{

	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(m_pButtonArray[i]->GetButtonCommand() == nCommand)
		{
			CToolbarButton *pOldButton = m_pButtonArray[i];
			m_pButtonArray[i] = pNewButton;
			pOldButton->ShowWindow(SW_HIDE);
			pOldButton->SetParent(NULL);
			pOldButton->SetOwner(NULL);

			pNewButton->SetParent(this);
			pNewButton->SetOwner(this);
			pNewButton->ShowWindow(SW_SHOWNA);

			if(CheckMaxButtonSizeChanged(pNewButton, TRUE))
			{
				ChangeButtonSizes();
				LayoutButtons(-1);
				GetParentFrame()->RecalcLayout();
			}
			else
			{
				//make sure it's the size of the largest button so far
				pNewButton->SetButtonSize(CSize(m_nMaxButtonWidth, m_nMaxButtonHeight));
				LayoutButtons(i - 1);
			}

			return pOldButton;
		}
	}
	return NULL;

}

void CNSToolbar2::ReplaceButton(UINT nOldCommand, UINT nNewCommand)
{

	int nCount = m_pHiddenButtons.GetSize();

	for(int i = 0; i < nCount; i++)
	{
		CToolbarButton *pButton = (CToolbarButton*)m_pHiddenButtons[i];

		if(pButton->GetButtonCommand() == nNewCommand)
		{
			m_pHiddenButtons.RemoveAt(i, 1);
			CToolbarButton *pHideButton = ReplaceButton(nOldCommand, pButton);
			if(pHideButton)
				m_pHiddenButtons.Add(pHideButton);
		}
	}

}


CToolbarButton* CNSToolbar2::GetNthButton(int nIndex)
{
	ASSERT( nIndex < m_nMaxButtons);
	ASSERT( nIndex >=0 && nIndex < m_nNumButtons);

	return(m_pButtonArray[nIndex]);
}

CToolbarButton* CNSToolbar2::GetButton(UINT nID)
{
	int nIndex = FindButton(nID);

	if(nIndex != -1)
	{
		return m_pButtonArray[nIndex];
	}
	else
		return NULL;

}

int CNSToolbar2::GetNumButtons(void)
{
	return m_nNumButtons;
}

void CNSToolbar2::SetBitmap(UINT nBitmapID)
{
	HINSTANCE hInst = AfxGetResourceHandle();

	if(m_hBitmap != NULL)
		::DeleteObject(m_hBitmap);

	if(nBitmapID == 0)
		return;

	m_nBitmapID = nBitmapID;

	CDC *pDC = GetDC();
	WFE_InitializeUIPalette(pDC->GetSafeHdc());

	m_hBitmap = WFE_LoadTransparentBitmap(hInst, pDC->m_hDC, 
				sysInfo.m_clrBtnFace, RGB(255, 0, 255), WFE_GetUIPalette(GetParentFrame()), nBitmapID);
	ReleaseDC(pDC);

}

void CNSToolbar2::SetBitmap(char *pBitmapFile)
{
	HINSTANCE hInst = AfxGetResourceHandle();

	if(m_hBitmap != NULL)
		::DeleteObject(m_hBitmap);

	m_hBitmap = WFE_LoadBitmapFromFile(pBitmapFile);
}



HBITMAP CNSToolbar2::GetBitmap(void)
{
	return m_hBitmap;
}

// Function - ChangeSize
//
// Changes the mode of the toolbar and changes the mode of each button and then
// lays each button out again
// 
// Arguments -  bNoviceMode		whether or not we are in novice mode
//
// Returns - nothing
void CNSToolbar2::SetToolbarStyle(int nToolbarStyle)
{
	if(m_nToolbarStyle != nToolbarStyle)
	{
		m_nToolbarStyle = nToolbarStyle;

		int i;

		for(i = 0; i < m_nNumButtons; i++)
		{
			m_pButtonArray[i]->SetButtonMode(nToolbarStyle);
		}

		int nCount = m_pHiddenButtons.GetSize();

		for(i = 0; i < nCount; i++)
		{
			((CToolbarButton*)m_pHiddenButtons[i])->SetButtonMode(nToolbarStyle);
		}

		FindLargestButton();
		ChangeButtonSizes();

		m_nHeight = GetHeight();

		LayoutButtons(-1);
		RedrawWindow();
	}
}

void CNSToolbar2::SetBitmapSize(int nWidth, int nHeight)
{
	for(int i = 0; i < m_nNumButtons; i++)
	{
		m_pButtonArray[i]->SetBitmapSize( CSize(nWidth, nHeight));
	}

	FindLargestButton();
	ChangeButtonSizes();
	LayoutButtons(-1);

}

void CNSToolbar2::LayoutButtons(void)
{
	LayoutButtons(-1);
}

int CNSToolbar2::GetHeight(void)
{
	return m_nMaxButtonHeight;
}

int CNSToolbar2::GetWidth(void)
{
	return m_nWidth;
}

void CNSToolbar2::SetButtonsSameWidth(BOOL bButtonsSameWidth)
{
	m_bButtonsSameWidth = bButtonsSameWidth;
	LayoutButtons(-1);
}

void CNSToolbar2::ReplaceButtonBitmapIndex(UINT nID, UINT nIndex)
{

	int nButtonIndex = FindButton(nID);

	if(nButtonIndex != -1)
	{
		CToolbarButton *pButton = m_pButtonArray[nButtonIndex];
		pButton->ReplaceBitmapIndex(nIndex);
	}

}


void CNSToolbar2::GetButtonXPosition(int nSelection,int & nStart,int & nEnd)
{
	if(nSelection >= 0 && nSelection < m_nNumButtons && m_pButtonArray[nSelection] != NULL)
	{
		CRect buttonRect;

		m_pButtonArray[nSelection]->GetClientRect(&buttonRect);
		m_pButtonArray[nSelection]->MapWindowPoints(this, &buttonRect);

		nStart = buttonRect.left;
		nEnd = buttonRect.right;
	}
}

// If bIsBefore is TRUE, finds the button before the one that occurs completely after boundary
// if bIsBefore is FALSE, finds the button after the one the occurs completely before boundary
// returns -1 if nothing fulfills these conditions
int CNSToolbar2::FindButtonFromBoundary(int boundary, BOOL bIsBefore)
{

	for(int i = 0; i < m_nNumButtons; i++)
	{
		CRect buttonRect;

		m_pButtonArray[i]->GetClientRect(&buttonRect);
		m_pButtonArray[i]->MapWindowPoints(this, &buttonRect);
		
		if(bIsBefore)
		{
			if(buttonRect.left >= boundary)
			{
				return(i == 0 ? -1 : i - 1);
			}
		}
		else
		{
			if(buttonRect.right >= boundary)
				return i;
		}
	}

	return -1;

}


void CNSToolbar2::SetCustomizableToolbar(CCustToolbar* pCustToolbar)
{
	m_pCustToolbar = pCustToolbar;
}

CCustToolbar *CNSToolbar2::GetCustomizableToolbar(void)
{

	return m_pCustToolbar;
}

BOOL CNSToolbar2::OnCommand( WPARAM wParam, LPARAM lParam )
{

	return((BOOL)GetParentFrame()->SendMessage(WM_COMMAND, wParam, lParam));
}

void CNSToolbar2::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{
	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(m_pButtonArray[i]->NeedsUpdate())
		{
			m_pButtonArray[i]->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
		}
	}
}

void CNSToolbar2::UpdateURLBars(char* url)
{
	for(int i = 0; i < m_nNumButtons; i++)
	{
		m_pButtonArray[i]->UpdateURLBar(url);
	}
}

BOOL CNSToolbar2::OnNotify( WPARAM wParam, LPARAM lParam, LRESULT* pResult )
{
	NMHDR *hdr = (NMHDR*)lParam;
	if(hdr->code == TTN_NEEDTEXT)
	{
		int command = (int) wParam;
		UINT hi = HIWORD(wParam);
		UINT type = TTN_NEEDTEXT;
		return TRUE;
	}
#ifdef _WIN32
	else
		return (CWnd::OnNotify(wParam, lParam, pResult));
#else
	return FALSE;
#endif
}

BOOL CNSToolbar2::SetDoOnButtonDownByCommand(UINT nCommand, BOOL bDoOnButtonDown)
{

	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(m_pButtonArray[i]->GetButtonCommand() == nCommand)
		{
			m_pButtonArray[i]->SetDoOnButtonDown(bDoOnButtonDown);
            return TRUE;
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//					Messages for CNSToolbar2
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CNSToolbar2, CWnd)
	//{{AFX_MSG_MAP(CWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(NSBUTTONDRAGGING, OnButtonDrag)
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_MESSAGE(TB_SIZECHANGED, OnToolbarButtonSizeChanged)
	ON_WM_PALETTECHANGED()
	ON_WM_SYSCOLORCHANGE()
	//}}AFX_MSG_MAP

END_MESSAGE_MAP()

// Function - OnLButtonDown
//
// If the mouse click occured on a button then we may need to move the button
// Otherwise we pass the click on to our parent because we may need to move the
// toolbar
// 
// Arguments -  none
//
// Returns - nothing
void CNSToolbar2::OnLButtonDown(UINT nFlags, CPoint point)
{
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
}

void CNSToolbar2::OnMouseMove(UINT nFlags, CPoint point)
{
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x, point.y));
}

void CNSToolbar2::OnLButtonUp(UINT nFlags, CPoint point)
{
	MapWindowPoints(GetParent(), &point, 1);
	GetParent()->SendMessage(WM_LBUTTONUP, nFlags, MAKELPARAM(point.x, point.y));
}

LRESULT CNSToolbar2::OnButtonDrag(WPARAM wParam, LPARAM lParam)
{
	HWND hWnd = (HWND) lParam;

	CWnd *pButton = CWnd::FromHandle(hWnd);

	int nButtonIndex = FindButton(pButton);

	if(nButtonIndex != -1)
	{
		MoveButton(nButtonIndex);
	}

	return 1;
}

void CNSToolbar2::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CNSToolbar2::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}

void CNSToolbar2::OnPaint(void)
{
	CRect rcClient, updateRect, buttonRect, intersectRect;
	
	GetClientRect(&rcClient);
	GetUpdateRect(&updateRect);

	CPaintDC dcPaint(this);
	
	// Use our background color
	::FillRect(dcPaint.m_hDC, &rcClient, sysInfo.m_hbrBtnFace);

	for (int i = 0; i < m_nNumButtons; i++)
	{
		m_pButtonArray[i]->GetClientRect(&buttonRect);

		m_pButtonArray[i]->MapWindowPoints(this, &buttonRect);

		if(intersectRect.IntersectRect(updateRect, buttonRect))
		{
			MapWindowPoints(m_pButtonArray[i], &intersectRect);
			m_pButtonArray[i]->RedrawWindow(&intersectRect);
		}

	}

}

LRESULT CNSToolbar2::OnToolbarButtonSizeChanged(WPARAM wParam, LPARAM lParam)
{

	HWND hWnd = (HWND) lParam;

	CToolbarButton *pButton = (CToolbarButton*)CWnd::FromHandle(hWnd);

	int nButtonIndex = FindButton(pButton);

	if(nButtonIndex != -1)
	{
		if(CheckMaxButtonSizeChanged(pButton, TRUE) || CheckMaxButtonSizeChanged(pButton, FALSE))
		{
			ChangeButtonSizes();
			LayoutButtons(-1);
			GetParentFrame()->RecalcLayout();
			GetParent()->SendMessage(CASTUINT(TOOLBAR_WIDTH_CHANGED), 0, LPARAM(m_hWnd));
		}
	}


	return 1;
}



void CNSToolbar2::OnPaletteChanged( CWnd* pFocusWnd )
{
	if (pFocusWnd != this) {
		Invalidate();
	}
}
  
void CNSToolbar2::OnSysColorChange( )
{
	if(m_nBitmapID != 0)
	{
		SetBitmap(m_nBitmapID);

		CToolbarButton *pButton;

		for(int i = 0; i < m_nNumButtons; i++)
		{
			pButton = m_pButtonArray[i];
			pButton->SetBitmap(m_hBitmap, TRUE);
		}

		int nHiddenSize = m_pHiddenButtons.GetSize();
		for(int j = 0; j < nHiddenSize; j++)
		{
			pButton = (CToolbarButton*)m_pHiddenButtons[j];
			pButton->SetBitmap(m_hBitmap, TRUE);
		}

		Invalidate();
	}
}


//////////////////////////////////////////////////////////////////////////
//						Helper Functions for CNSToolbar2
//////////////////////////////////////////////////////////////////////////

// Function - LayoutButtons
//
// Lays out the buttons starting with nStartIndex + 1.  If nStartIndex is not
// 0, this uses nStartIndex's position to figure out the starting point for the
// rest of the buttons.  If it's 0 then it just starts laying them out from 
// LEFT_TOOLBAR_MARGIN
// 
// Arguments - nStartIndex		the index we will start laying out from
//
// Returns - nothing
void CNSToolbar2::LayoutButtons(int nStartIndex)
{
	int nStartX = LEFT_TOOLBAR_MARGIN;
	RECT buttonRect;
	CSize buttonSize;

	// if it's not 0 then we can use the previous one as a reference point
	if(nStartIndex >= 0)
	{
		m_pButtonArray[nStartIndex]->GetClientRect(&buttonRect);
		m_pButtonArray[nStartIndex]->MapWindowPoints(this, &buttonRect);

		nStartX = buttonRect.right + SPACE_BETWEEN_BUTTONS;
	}

	for( int i = nStartIndex + 1; i< m_nNumButtons; i++)
	{
		buttonSize = m_pButtonArray[i]->GetButtonSize();

		m_pButtonArray[i]->MoveWindow(nStartX, (m_nHeight - buttonSize.cy) / 2,
									  buttonSize.cx, buttonSize.cy);

		nStartX += buttonSize.cx + SPACE_BETWEEN_BUTTONS;

	}
	//record the width of our toolbar
	m_nWidth = nStartX;
}

// Return the rect of the button with nID in screen coordinates
// Returns TRUE if the button exists, FALSE if it doesn't.
BOOL CNSToolbar2::GetButtonRect(UINT nID, RECT *pRect)
{
	int nIndex = FindButton(nID);

	if(nIndex != -1)
	{
		m_pButtonArray[nIndex]->GetWindowRect(pRect);
		return TRUE;
	}
	else
		return FALSE;

}

// Function - FindButton
//
// Given a point, finds out what button lies underneath that point
// 
// Arguments - point		the point we are looking at
//
// Returns - the index of the button or -1 if the point does not fall over
//			 a button


int CNSToolbar2::FindButton(CPoint point)
{

	RECT buttonRect;

	for(int i =0 ; i < m_nNumButtons; i++)
	{
		m_pButtonArray[i]->GetClientRect(&buttonRect);
		m_pButtonArray[i]->MapWindowPoints(this, &buttonRect);

		if(point.x < buttonRect.left)
		{
			return -1;
		}

		if(point.x <= buttonRect.right && point.y >= buttonRect.top &&
			point.y <= buttonRect.bottom)
		{
			return i;
		}
	}

	return -1;
}

// Function - FindButton
//
// Given a button, finds out what index it is in the button array
// 
// Arguments - pButton		the button whose index we are looking for
//
// Returns - the index of the button or -1 if the button does not exist
//			 a button
int  CNSToolbar2::FindButton(CWnd *pButton)
{

	for(int i = 0; i< m_nNumButtons; i++)
	{
		if(m_pButtonArray[i] == pButton)
		{
			return i;
		}
	}
	
	return -1;
}

int	 CNSToolbar2::FindButton(UINT nCommand)
{
	for(int i = 0; i< m_nNumButtons; i++)
	{
		if(m_pButtonArray[i]->GetButtonCommand() == nCommand)
		{
			return i;
		}
	}
	
	return -1;
}

void CNSToolbar2::MoveButton(int nIndex)
{
	ASSERT(nIndex >= 0 && nIndex <m_nNumButtons);

    COleDataSource * pDataSource = new COleDataSource;  


    m_pButtonArray[nIndex]->FillInOleDataSource(pDataSource);

	CToolbarButton *pButton = m_pButtonArray[nIndex];

    // Don't start drag until outside this rect 

    RECT rectDragStart;
	pButton->GetClientRect(&rectDragStart);
	pButton->MapWindowPoints(this, &rectDragStart);

	DROPEFFECT effect;
	CToolbarDropSource * pDropSource = new CToolbarDropSource;

    effect=pDataSource->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE | DROPEFFECT_SCROLL | DROPEFFECT_NONE,
                            &rectDragStart, pDropSource);
	

	delete pDropSource;
	delete pDataSource;

}

BOOL CNSToolbar2::CheckMaxButtonSizeChanged(CToolbarButton *pButton, BOOL bAdd)
{
	CSize size = pButton->GetRequiredButtonSize();
	BOOL bChanged = FALSE;

	if(bAdd)
	{
		if(m_bButtonsSameWidth)
		{
			if(size.cx > m_nMaxButtonWidth)
			{
				m_nMaxButtonWidth = size.cx;
				bChanged = TRUE;
			}
		}

		if(size.cy > m_nMaxButtonHeight)
		{
			m_nMaxButtonHeight = size.cy;
			m_nHeight = size.cy;
			bChanged = TRUE;
		}
	}
	else
	{
		if((size.cx == m_nMaxButtonWidth && m_bButtonsSameWidth) || size.cy == m_nMaxButtonHeight)
		{
			if(FindLargestButton())
				bChanged = TRUE;
		}
		
	}
	return bChanged;

}

void CNSToolbar2::ChangeButtonSizes(void)
{
	int nWidth;

	for(int i = 0; i < m_nNumButtons; i++)
	{
		if(!m_bButtonsSameWidth)
		{
			CSize size = m_pButtonArray[i]->GetRequiredButtonSize();
			nWidth = size.cx;
		}
		else
			nWidth = m_nMaxButtonWidth;

		m_pButtonArray[i]->SetButtonSize(CSize(nWidth, m_nMaxButtonHeight));
	}
}

// returns TRUE if either max height or max width changed
BOOL CNSToolbar2::FindLargestButton(void)
{
	int nOldMaxWidth = m_nMaxButtonWidth;
	int nOldMaxHeight = m_nMaxButtonHeight;

	m_nMaxButtonHeight = 0;

	m_nMaxButtonWidth = 0;

	for(int i = 0; i < m_nNumButtons; i++)
	{
		CheckMaxButtonSizeChanged(m_pButtonArray[i], TRUE);
	}

	return (((nOldMaxWidth != m_nMaxButtonWidth) && m_bButtonsSameWidth) || nOldMaxHeight != m_nMaxButtonHeight);

}
// End CNSToolbar2 implementation





///////////////////////////////////////////////////////////////////////////
//							Class CCommandToolbarDropTarget
///////////////////////////////////////////////////////////////////////////

DROPEFFECT CCommandToolbarDropTarget::OnDragEnter(CWnd * pWnd,	COleDataObject * pDataObject,
											   DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// We are interested in command buttons
	if(pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_COMMAND_BUTTON_FORMAT)))
	{
		deReturn = DROPEFFECT_MOVE;
	}

	return(deReturn);
	
}

DROPEFFECT CCommandToolbarDropTarget::OnDragOver(CWnd * pWnd, COleDataObject * pDataObject,
											  DWORD dwKeyState, CPoint point )
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;

	// We are interested in bookmark buttons
	if(pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_COMMAND_BUTTON_FORMAT)))
	{
		deReturn = DROPEFFECT_MOVE;
	}

	return(deReturn);
	
}

BOOL CCommandToolbarDropTarget::OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	// We're interested in bookmark buttons
	CLIPFORMAT cfCommandButton = ::RegisterClipboardFormat(NETSCAPE_COMMAND_BUTTON_FORMAT);
	if (pDataObject->IsDataAvailable(cfCommandButton))
	{

		HGLOBAL hCommandButton = pDataObject->GetGlobalData(cfCommandButton);
		LPCOMMANDBUTTONITEM pCommandButtonItem = (LPCOMMANDBUTTONITEM)::GlobalLock(hCommandButton);

		ASSERT(pCommandButtonItem != NULL);

		BUTTONITEM buttonInfo = pCommandButtonItem->buttonInfo;
		::GlobalUnlock(hCommandButton);

		CCommandToolbarButton *pCommandButton = (CCommandToolbarButton*)CWnd::FromHandlePermanent(buttonInfo.button);

		((CNSToolbar2*)pWnd)->AddButtonAtPoint(pCommandButton, point);
		return TRUE;

	}
	
	return(bRtn);
}

// End CCommandToolbarDropTarget implementation

///////////////////////////////////////////////////////////////////////////
//							Class CCommandToolbar
///////////////////////////////////////////////////////////////////////////
#define COMMANDTOOLBARSMALLHEIGHT 21

CCommandToolbar::CCommandToolbar(int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, 
								 int nPicturesHeight, int nTextHeight)
	 : CNSToolbar2(nMaxButtons, nToolbarStyle, nPicturesAndTextHeight, nPicturesHeight, nTextHeight)
{
	
}

int CCommandToolbar::Create(CWnd *pParent)
{

	int result = CNSToolbar2::Create(pParent);

	return result;
}

int CCommandToolbar::GetHeight(void)
{

	int nHeight = CNSToolbar2::GetHeight();

	if(m_nToolbarStyle != TB_PICTURESANDTEXT)
		nHeight = (COMMANDTOOLBARSMALLHEIGHT > nHeight) ? COMMANDTOOLBARSMALLHEIGHT : nHeight;
      
	return nHeight;

}

///////////////////////////////////////////////////////////////////////////
//						CCommandToolbarBar Messages
///////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CCommandToolbar, CNSToolbar2)
    ON_WM_CREATE ( )
END_MESSAGE_MAP()

int CCommandToolbar::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = CNSToolbar2::OnCreate ( lpCreateStruct );

	m_DropTarget.Register(this);
	DragAcceptFiles(FALSE);
	return iRetVal;

}

///////////////////////////////////////////////////////////////////////////
//							Class CToolbarControlBar
///////////////////////////////////////////////////////////////////////////

CToolbarControlBar::CToolbarControlBar(void)
{
	m_bEraseBackground = TRUE;
	m_pToolbar = NULL;
}

CToolbarControlBar::~CToolbarControlBar(void)
{
	if(m_pToolbar)
		delete m_pToolbar;
}

// Function	- Create
//
// Given a parent CFrameWnd, this creates the control bar that will hold all of the tabbed
// toolbars
// 
// Arguments - pParent		the parent frame window
//
// Returns - Nonzero if successful, 0 if unsuccessful
int CToolbarControlBar::Create(CFrameWnd *pParent, DWORD dwStyle, UINT nID )
{
	CRect rect;

	ASSERT_VALID(pParent);   // must have a parent

//	dwStyle &= ~CBRS_ALL;

#ifdef _WIN32
	dwStyle |= CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC;
	m_dwStyle = dwStyle;
#else
	dwStyle = 0x2000; //Align top
#endif

	// create the HWND
	rect.SetRectEmpty();
	CBrush brush;
	if (!CWnd::Create(theApp.NSToolBarClass, NULL, dwStyle | WS_VISIBLE| WS_CLIPCHILDREN, rect,
		pParent, nID))
	{
		return 0;
	}
	// Note: Parent must resize itself for control bar to be resized
#ifdef _WIN32
	pParent->ShowControlBar(this, TRUE, FALSE);
	pParent->RecalcLayout();

#else
	ASSERT("That's not" == "16 bit");
#endif
	return 1;
}

CSize CToolbarControlBar::CalcDynamicLayout(int nLength, DWORD dwMode )
{
	if(m_pToolbar)
		return CSize(32767, m_pToolbar->GetHeight());
	else
		return CSize(32767, 0);

}

void CToolbarControlBar::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler )
{
	if(m_pToolbar)
		m_pToolbar->OnUpdateCmdUI(pTarget, bDisableIfNoHndler);
}

BEGIN_MESSAGE_MAP(CToolbarControlBar, CControlBar)
	//{{AFX_MSG_MAP(CWnd)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
#ifndef WIN32
	ON_MESSAGE(WM_SIZEPARENT, OnSizeParent)
#endif

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CToolbarControlBar::OnSize( UINT nType, int cx, int cy )
{
	CWnd *pChild = GetWindow(GW_CHILD);

	if(pChild)
	{
		pChild->MoveWindow(0, 0, cx, cy, FALSE);
	}
}

void CToolbarControlBar::OnPaint(void)
{
	CRect rcClient, updateRect, toolbarRect, intersectRect;

	GetClientRect(&rcClient);
	GetUpdateRect(&updateRect);
	m_pToolbar->GetClientRect(&toolbarRect);

	CPaintDC dcPaint(this);

	// Use our background color
	::FillRect(dcPaint.m_hDC, &rcClient, sysInfo.m_hbrBtnFace);

#ifdef _WIN32
	DrawBorders(&dcPaint, rcClient);
#endif

	m_pToolbar->MapWindowPoints(this, &toolbarRect);

	if(intersectRect.IntersectRect(updateRect, toolbarRect))
	{
		MapWindowPoints(m_pToolbar, &intersectRect);
		m_pToolbar->RedrawWindow(&intersectRect);
	}
}

void CToolbarControlBar::OnShowWindow( BOOL bShow, UINT nStatus )
{
	m_bEraseBackground = bShow;
}

BOOL CToolbarControlBar::OnEraseBkgnd( CDC* pDC )
{
	if ( m_bEraseBackground ) {
		m_bEraseBackground = FALSE;
		return (BOOL) Default();
	} else {
		return TRUE;
	}
}

#ifndef WIN32
LRESULT CToolbarControlBar::OnSizeParent(WPARAM wParam, LPARAM lParam)
{
	m_sizeFixedLayout = CalcDynamicLayout(0, 0);
	return CControlBar::OnSizeParent(wParam, lParam);
}
#endif
