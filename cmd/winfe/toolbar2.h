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

// Creates a class that will represent the new toolbar which can hold
// the new buttons for the customizable toolbar.


#ifndef _TOOLBAR2_H
#define _TOOLBAR2_H

#include <afxwin.h>
#include <afxext.h>
#include <afxpriv.h>
#include <afxole.h>
#include <afxdisp.h>
#include <afxodlgs.h>
#ifdef _WIN32
#include <afxcmn.h>
#endif

#include "tlbutton.h"
#include "csttlbr2.h"

#define TOOLBAR_WIDTH_CHANGED (WM_USER + 31)
#define TOOLBAR_BUTTON_ADD (WM_USER + 40)
#define TOOLBAR_BUTTON_REMOVE (WM_USER + 41)
#define TOOLBAR_BUTTON_DELETED (WM_USER + 42)

class CToolbarDropSource : public COleDropSource {
public:
	CToolbarDropSource() {}
	SCODE GiveFeedback( DROPEFFECT dropEffect);

};

class CCustToolbar;


class CNSToolbar2 : public CWnd {
//DECLARE_DYNCREATE(CNSToolbar2)

protected:
	BOOL			 m_bEraseBackground;

	int				 m_nMaxButtons;		// The maximum number of buttons this toolbar can hold
	BOOL			 m_nToolbarStyle;		// whether we are in novice or advanced mode
	int				 m_nPicturesAndTextHeight;	// Our height when we are in novice mode
	int				 m_nPicturesHeight;
	int				 m_nTextHeight;	// Our height when we are in advanced mode
	CToolbarButton** m_pButtonArray;	// The array of toolbar buttons
	int				 m_nNumButtons;		// The number of buttons we currently hold
	int				 m_nHeight;			// Our current height
	int				 m_nWidth;			// Our current required width given all of the buttons

	BOOL			 m_bDragging;		// Are we currently dragging a button
	int				 m_nDraggingButton; // Index of button we are dragging
	CPoint	 		 m_draggingPoint;	// Point we started dragging from
	CCustToolbar*    m_pCustToolbar;	// A pointer to the customizable toolbar it belongs to
	
	int				 m_nMaxButtonWidth; // The width of the widest button
	int				 m_nMaxButtonHeight;// The height of the tallest button
	HBITMAP			 m_hBitmap;			// Bitmap to be used for toolbar (doesn't have to have one);
	CPtrArray		 m_pHiddenButtons;  // Buttons we are keeping track of but aren't currently showing;
	BOOL			 m_bButtonsSameWidth;// Are all the buttons the width of the largest one or
										// their own individual widths
	UINT			 m_nBitmapID;		// The toolbar's bitmap
public:
	CNSToolbar2(){}
	CNSToolbar2(int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, int nPicturesHeight, int nTextHeight);
	~CNSToolbar2();

	int Create(CWnd *pParent);

	virtual void AddButtonAtIndex(CToolbarButton *pButton, int index = -1, BOOL bNotify = TRUE);
	virtual int AddButtonAtPoint(CToolbarButton *pButton, CPoint point, BOOL bAddButton = TRUE);
    
    virtual void AddButton(CToolbarButton* pButton, int index = -1) { AddButtonAtIndex(pButton, index); }

	void AddHiddenButton(CToolbarButton *pButton);

	CToolbarButton* RemoveButton(int nIndex, BOOL bNotify = TRUE);
	CToolbarButton* RemoveButton(CToolbarButton *pButton);
	CToolbarButton* RemoveButtonByCommand(UINT nCommand);

	void DecrementButtonCount() { m_nNumButtons--; }

	void HideButtonByCommand(UINT nCommand);
	void ShowButtonByCommand(UINT nCommand, int nPos);

	void RemoveAllButtons(void);
	CToolbarButton *ReplaceButton(UINT nCommand, CToolbarButton *pNewButton);
	void ReplaceButton(UINT nOldCommand, UINT nNewCommand);

	CToolbarButton* GetNthButton(int nIndex);
	CToolbarButton* GetButton(UINT nID);  //Added by cmanske

	int GetNumButtons(void);

	void SetBitmap(UINT nBitmapID);
	void SetBitmap(char *pBitmapFile);
	HBITMAP GetBitmap(void);

	// Sets each buttons bitmap to be nWidth by nHeight;
	void SetBitmapSize(int nWidth, int nHeight);

	void SetToolbarStyle(int nToolbarStyle);
	BOOL GetToolbarStyle(void) {return m_nToolbarStyle;}
	void LayoutButtons(void);
    virtual void LayoutButtons(int nStartIndex);
    virtual void WidthChanged(int width) {};  // used by personal toolbar.

	BOOL GetButtonRect(UINT nID, RECT *pRect);

	void SetCustomizableToolbar(CCustToolbar* pCustToolbar);
	CCustToolbar *GetCustomizableToolbar(void);

	virtual int GetHeight(void);
	int GetWidth(void);

	void SetButtonsSameWidth(BOOL bButtonsSameWidth);
	BOOL GetButtonsSameWidth(void) { return m_bButtonsSameWidth; }

	void ReplaceButtonBitmapIndex(UINT nID, UINT nIndex);

	BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	virtual BOOL OnNotify( WPARAM wParam, LPARAM lParam, LRESULT* pResult );

	void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );

    // Set a button to do its command on button down instead of button up
    BOOL SetDoOnButtonDownByCommand(UINT nCommand, BOOL bDoOnButtonDown);

	// Generated message map functions
	//{{AFX_MSG(CNSToolbar2)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnButtonDrag(WPARAM, LPARAM); 
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnPaint(void);
	afx_msg LRESULT OnToolbarButtonSizeChanged(WPARAM, LPARAM);
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );
	afx_msg void OnSysColorChange( );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	int  FindButton(CPoint point);
	int  FindButton(CWnd *pButton);
	int	 FindButton(UINT nCommand);
	void MoveButton(int nIndex);
	BOOL CheckMaxButtonSizeChanged(CToolbarButton *pButton, BOOL bAdd);
	void ChangeButtonSizes(void);
	BOOL FindLargestButton(void);
	//Given an index, this says where a button starts and where it ends
	void GetButtonXPosition(int nSelection,int & nStart,int & nEnd);
	int FindButtonFromBoundary(int boundary, BOOL bIsBefore);






};



class CCommandToolbarDropTarget : public COleDropTarget
{
	public:
		CCommandToolbarDropTarget() {}
	 
	protected:
		virtual DROPEFFECT OnDragEnter(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT OnDragOver(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point );
		virtual BOOL OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point);

	private:

};

class CCommandToolbar : public CNSToolbar2 {

//private:
	CCommandToolbarDropTarget m_DropTarget;

public:
	CCommandToolbar(int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, int nPicturesHeight,
					int nTextHeight);

	int Create(CWnd *pParent);
	virtual int GetHeight(void);

	// Generated message map functions
	//{{AFX_MSG(CCommandToolbar)
		afx_msg int OnCreate ( LPCREATESTRUCT );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


class CToolbarControlBar : public CControlBar {

protected:
	CNSToolbar2 *m_pToolbar;
	BOOL m_bEraseBackground;

public:
	CToolbarControlBar();
	~CToolbarControlBar();

	//Creation
	int Create(CFrameWnd *pParent, DWORD dwStyle, UINT nID );

	//Positioning/Resizing
	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode ); 

	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );

	void SetToolbar(CNSToolbar2 *pToolbar) { m_pToolbar = pToolbar; }
	CNSToolbar2* GetToolbar(void) { return m_pToolbar; }

	// Generated message map functions
	//{{AFX_MSG(CToolbarControlBar)
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnPaint(void);
#ifndef WIN32
	afx_msg LRESULT OnSizeParent(WPARAM wParam, LPARAM lParam);
#endif
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
 };

#endif
