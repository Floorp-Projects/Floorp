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

// Defines a toolbar button
// Created August 19, 1996 by Scott Putterman


#ifndef _TLBUTTON_H
#define _TLBUTTON_H

#include "winproto.h"

/* Needed for IconType enum */
#include "rdfacc.h"

#include "tooltip.h"

#define SHOW_ALL_CHARACTERS -1

#define TB_DYNAMIC_TOOLTIP		0x1		//Client fills in tooltip
#define TB_HAS_TIMED_MENU		0x2		//Popup menu shows up on left mouse down after a small period of time
#define TB_HAS_IMMEDIATE_MENU	0x4		//Popup menu shows up on left mouse down immediately
#define TB_HAS_DRAGABLE_MENU	0x8		//Popup menu uses a CDropMenu instead of an HMENU. In order for this
										//feature to work, must call SetDropTarget before dragging occurs. And
										//the drop target must be derived from CToolbarButtonDropTarget
#define TB_DYNAMIC_STATUS		0x10	//Client fills in status message

#define TB_HAS_DROPDOWN_TOOLBAR	0x20		// A CDropDownToolbar shows up on left mouse down (used for Composer only)

#define NSBUTTONMENUOPEN   (WM_USER + 1)	// Sent when a button menu is opened and needs to be filled in
#define NSBUTTONDRAGGING   (WM_USER + 2)	// Sent when a button is being dragged
#define NSDRAGMENUOPEN	   (WM_USER + 3)	// Sent when a CDrop menu is being opened and needs to be filled in 
#define NSDRAGGINGONBUTTON (WM_USER + 4)	// Sent when something is being dragged ontop of a button

#define TB_FILLINTOOLTIP   (WM_USER + 8)	// Sent when a string for a tooltip needs to be filled in
#define TB_FILLINSTATUS	   (WM_USER + 9)	// Sent when a string for the status bar needs to be filled in
#define TB_SIZECHANGED	   (WM_USER + 30)	// Sent when the button changes size

typedef struct {
	HWND button;
} BUTTONITEM;

typedef BUTTONITEM * LPBUTTONITEM;

#define NETSCAPE_BOOKMARK_BUTTON_FORMAT  "Netscape Bookmark Button Format"

typedef struct {
    BOOKMARKITEM bookmark;
	BUTTONITEM	 buttonInfo;
} BOOKMARKBUTTONITEM;

typedef BOOKMARKBUTTONITEM * LPBOOKMARKBUTTONITEM;

#define NETSCAPE_COMMAND_BUTTON_FORMAT  "Netscape Command Button Format"

typedef struct {
    UINT		 nCommand;
	BUTTONITEM	 buttonInfo;
} COMMANDBUTTONITEM;

typedef COMMANDBUTTONITEM * LPCOMMANDBUTTONITEM;

class CToolbarButton;

class CButtonEditWnd : public CEdit
{
protected:
	CToolbarButton *m_pOwner;	
public:
    CButtonEditWnd() { m_pOwner = NULL; }
    ~CButtonEditWnd();

	void SetToolbarButtonOwner(CToolbarButton *pOwner);
    virtual BOOL PreTranslateMessage ( MSG * msg );

protected:
		// Generated message map functions
	//{{AFX_MSG(CButtonEditWnd)
	afx_msg void OnDestroy();
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

class CToolbarButtonCmdUI : public CCmdUI        
{
public: // re-implementations only
	virtual void Enable(BOOL bOn);
	virtual void SetCheck( int nCheck = 1 );
 

};

class CDropMenu;
class CToolbarButton;

#define CToolbarButtonDropTargetBase	COleDropTarget

class CToolbarButtonDropTarget : public CToolbarButtonDropTargetBase
{
protected:
	CToolbarButton *m_pButton;

public:
		CToolbarButtonDropTarget(){m_pButton = NULL;}
		void SetButton(CToolbarButton *pButton) { m_pButton = pButton;}

protected:
		virtual DROPEFFECT OnDragEnter(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT OnDragOver(CWnd * pWnd,
			COleDataObject * pDataObject, DWORD dwKeyState, CPoint point );
		virtual BOOL OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
			DROPEFFECT dropEffect, CPoint point);
		virtual DROPEFFECT ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point) = 0;
		virtual DROPEFFECT ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point) = 0;
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point) = 0;


	private:

}; 

class CToolbarButton: public CWnd {

public:
	// States for button effects
	enum BTN_STATE {eNORMAL, eDISABLED, eBUTTON_UP, eBUTTON_DOWN, eBUTTON_CHECKED};

protected:
	BOOL		m_bIsResourceID;		// does this use a resource or a file
	UINT		m_nBitmapID;			// the resource of the bitmap
	UINT		m_nBitmapIndex;		// the index of the bitmap we wish to show
	LPTSTR		m_pBitmapFile;		// the file in which the bitmap is stored
	LPTSTR		m_pButtonText;		// the text displayed in Novice mode
	LPTSTR		m_pToolTipText;		// the text displayed in the tooltip
	LPTSTR		m_pStatusText;		// the text displayed in the status bar
	int			m_nToolbarStyle;	// whether or not we are in Novice mode
	CSize		m_noviceButtonSize;	// the size of the button in Novice mode
	CSize		m_advancedButtonSize; // the size of the button in Advanced mode
	CSize		m_bitmapSize;			// The size of each bitmap in the resource
	int			m_nMaxTextChars;		// The maximum number of characters of the text shown on button
    int         m_nMinTextChars;        // The minimum number of characters shown on button
	DWORD		m_dwButtonStyle;		// the style of the button
	BTN_STATE	m_eState;				// Current button state
	BOOL		m_bEnabled;			// Is the button enabled
	BOOL		m_bNeedsUpdate;		// Does the button need to be disabled/enabled
	CToolbarButtonCmdUI m_state;
	CMenu		m_menu;				// The menu if the button has one
	CDropMenu*	m_pDropMenu;			// The drop menu if TB_HAS_DRAGABLE_MENU is set
	UINT		m_nCommand;			// command associated with this button
	UINT		m_hMenuTimer;			// The timer we use for starting menu if the button has one
	BOOL		m_bMenuShowing;		// The menu is showing;
	BOOL		m_bButtonDown;		// for every button up there must be a button down if an action 
									// is to be taken
	UINT		m_hFocusTimer;		// Timer to see if the button still has focus
	BOOL		m_bHaveFocus;		// Do we have focus
	HBITMAP		m_hBmpImg;			// The loaded bitmap
	BOOL		m_bBitmapFromParent;// Does the parent own the bitmap or do we have to free it.
	IconType	m_nIconType;			// The icon type (BITMAP, LOCAL_FILE, ARBITRARY URL)
	int			m_nChecked;			// Is this button checked
	CNSToolTip2 m_ToolTip;
	BOOL		m_bEraseBackground;
	CToolbarButtonDropTarget *m_pDropTarget;  //if we are a drop target
	BOOL		m_bDraggingOnButton;
	BOOL		m_bPicturesOnly;	// This button only shows pictures
    BOOL        m_bDoOnButtonDown;  // Command is executed on button down instead of usual button up
    CButtonEditWnd* m_pTextEdit;		// If the text can be edited in place we will need this edit window
    BOOL        atMinimalSize;      // TRUE if the button is at its minimal size
    BOOL        atMaximalSize;      // TRUE if the button is at its maximal size
	BOOL		m_bDepressed;		// Is the button depressed and locked.
	BOOL		hasCustomTextColor;	// TRUE if the button has a special text color.
	BOOL		hasCustomBGColor;	// TRUE if the button has a special background color.
	COLORREF	customTextColor;	// Custom Text Color
	COLORREF	customBGColor;		// Custom BG Color

public:

	CToolbarButton();
	~CToolbarButton();

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, UINT nBitmapID, UINT nBitmapIndex,
			   CSize m_bitmapSize, BOOL bNeedsUpdate, UINT nCommand, int nMaxTextChars, int nMinTextChars = 0,
               DWORD dwButtonStyle = 0);

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, LPCTSTR pBitmapFile,
			   CSize m_bitmapSize, BOOL bNeedsUpdate, UINT nCommand, int nMaxTextChars, int nMinTextChars = 0,
               DWORD dwButtonStyle = 0);

    void SetStyle(DWORD dwButtonStyle);
	void SetText(LPCTSTR pButtonText);
	const char *GetToolTipText(void) { return m_pToolTipText; }
	void SetToolTipText(LPCSTR pToolTipText);

	void SetStatusText(LPCSTR pStatusText);

	void SetBitmap(HBITMAP hBmpImg, BOOL bParentOwns);
	void ReplaceBitmap(UINT nBitmapID, UINT nBitmapIndex);
	void ReplaceBitmap(LPCTSTR pBitmapFile);
	void ReplaceBitmap(HBITMAP hBmpImg, UINT nBitmapIndex, BOOL bParentOwns);
	void ReplaceBitmapIndex(UINT nBitmapIndex);

	virtual void DrawCustomIcon(HDC hDC, int x, int y) {};
    virtual void DrawLocalIcon(HDC hDC, int x, int y) {};
	virtual CSize GetButtonSize(void);
	virtual CSize GetRequiredButtonSize(void);
    virtual CSize GetButtonSizeFromChars(CString s, int c);
    virtual CSize GetMaximalButtonSize(void);
    virtual CSize GetMinimalButtonSize(void);

    BOOL AtMinimalSize(void);
    BOOL AtMaximalSize(void);

    virtual void CheckForMinimalSize(void);
    virtual void CheckForMaximalSize(void);

	BOOL IsResourceID(void) { return m_bIsResourceID; }
	UINT GetBitmapID(void) { return m_nBitmapID; }
	UINT GetBitmapIndex(void) { return m_nBitmapIndex; }
	LPCTSTR GetBitmapFile(void) { return m_pBitmapFile; }
	CSize GetBitmapSize(void) { return m_bitmapSize; }
	int	 GetMaxTextCharacters(void) { return m_nMaxTextChars; }
    int GetMinTextCharacters(void) { return m_nMinTextChars; }

	BOOL NeedsUpdate(void) { return m_bNeedsUpdate; }

	void Enable(BOOL bEnabled) { m_bEnabled = bEnabled; }
	BOOL IsEnabled(void) { return m_bEnabled; }

	void SetCheck(int nCheck);
	int  GetCheck(void);

	void SetPicturesOnly(int bPicturesOnly);
	void SetButtonMode(int nToolbarStyle);
	void SetState(BTN_STATE eState) { m_eState = eState; }
	void SetMenuShowing(BOOL bMenuShowing) {m_bMenuShowing = bMenuShowing; }

	void SetBitmapSize(CSize sizeImage);
	void SetButtonSize(CSize buttonSize);

	void SetDropTarget(CToolbarButtonDropTarget *pDropTarget);
	CMenu & GetButtonMenu(void) { return m_menu; }
	UINT GetButtonCommand(void) { return m_nCommand; }
	void SetButtonCommand(UINT nCommand);

	virtual void OnAction(void) { }
	virtual void FillInOleDataSource(COleDataSource *pDataSource) {}
	
	void FillInButtonData(LPBUTTONITEM button);

	BOOL OnCommand(UINT wParam,LONG lParam);
	virtual BOOL OnNotify( WPARAM wParam, LPARAM lParam, LRESULT* pResult );

	virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler);

	//called when a button is being dragged
	void ButtonDragged(void);
    
    void SetDoOnButtonDown(BOOL bDoOnButtonDown) {m_bDoOnButtonDown = bDoOnButtonDown;}
    BOOL IsDoOnButtonDown() {return m_bDoOnButtonDown;}

    void AddTextEdit(void);
	void RemoveTextEdit(void);
	void SetTextEditText(char *pText);
	char *GetTextEditText(void);
	virtual void EditTextChanged(char *pText);

	void SetCustomColors(COLORREF textColor, COLORREF bgColor) { hasCustomBGColor = hasCustomTextColor = TRUE; customTextColor = textColor; customBGColor = bgColor; }
	virtual void UpdateIconInfo() {}

protected:
	virtual void DrawPicturesAndTextMode(HDC hDC, CRect rect);
	virtual void DrawPicturesMode(HDC hDC, CRect rect);
	virtual void DrawTextMode(HDC hDC, CRect rect);
	virtual void DrawBitmapOnTop(HDC hDC, CRect rect);
	virtual void DrawBitmapOnSide(HDC hDC, CRect rect);
	virtual void DrawTextOnly(HDC hDC, CRect rect);
	virtual void DrawButtonBitmap(HDC hDC, CRect rcImg);
	virtual void DrawButtonText(HDC hDC, CRect rcTxt, CSize sizeTxt, CString strTxt);
	virtual CSize GetBitmapOnTopSize(CString s, int c);
	virtual CSize GetBitmapOnSideSize(CString s, int c);
	virtual CSize GetBitmapOnlySize(void);
	virtual CSize GetTextOnlySize(CString s, int c);
	virtual void DrawUpButton(HDC hDC, CRect & rect);
	virtual void DrawDownButton(HDC hDC, CRect & rect);
	virtual void DrawCheckedButton(HDC hDC, CRect & rect);
	virtual void RemoveButtonFocus(void);
    virtual BOOL CreateRightMouseMenu(void);
	virtual void DisplayAndTrackMenu(void);
	virtual CWnd* GetMenuParent(void);
	virtual void GetTextRect(CRect &rect);
	virtual void GetPicturesAndTextModeTextRect(CRect &rect);
	virtual void GetPicturesModeTextRect(CRect &rect);
	virtual void GetTextModeTextRect(CRect &rect);
	virtual void GetBitmapOnTopTextRect(CRect &rect);
	virtual void GetTextOnlyTextRect(CRect &rect);
	virtual void GetBitmapOnSideTextRect(CRect &rect);

	virtual CPoint RequestMenuPlacement()
	{
		CPoint point(0, GetRequiredButtonSize().cy);
		ClientToScreen(&point);
		return point;
	}

	// Generated message map functions
	//{{AFX_MSG(CToolbarButton)
	afx_msg void OnPaint();
	virtual afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg UINT OnGetDlgCode( );
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnCaptureChanged( CWnd* pWnd );
	afx_msg LRESULT OnToolTipNeedText(WPARAM, LPARAM);
	virtual afx_msg void OnTimer( UINT  nIDEvent );
	afx_msg LRESULT OnDragMenuOpen(WPARAM, LPARAM);
	afx_msg LRESULT OnDraggingOnButton(WPARAM, LPARAM);
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );
	afx_msg void OnSysColorChange();
	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};


// This class represents a toolbar button that can be dragged via drag and drop
class CDragableToolbarButton: public CToolbarButton {

private:
	BOOL	m_bDragging;		//Are we being dragged
	CPoint  m_draggingPoint;    //The original point when we started dragging

public:
	CDragableToolbarButton();

	virtual void FillInOleDataSource(COleDataSource *pDataSource) = 0;

protected:
	// Generated message map functions
	//{{AFX_MSG(CDragableToolbarButton)
	virtual afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	virtual afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual afx_msg void OnTimer( UINT  nIDEvent );

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};

// This class represents a toolbar button that cannot be dragged via drag and drop
class CStationaryToolbarButton: public CToolbarButton {

public:
	CStationaryToolbarButton() {}

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, UINT nBitmapID, UINT nBitmapIndex,
			   CSize m_bitmapSize, BOOL bNeedsUpdate, UINT nCommand, int nMaxTextChars, 
			   DWORD dwButtonStyle = 0, int nMinTextChars = 0);

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, LPCTSTR pBitmapFile,
			   CSize m_bitmapSize, BOOL bNeedsUpdate, UINT nCommand, int nMaxTextChars, 
			   DWORD dwButtonStyle = 0, int nMinTextChars = 0);

protected:
	// Generated message map functions
	//{{AFX_MSG(CStationaryToolbarButton)
	virtual afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	virtual afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};



class CCommandToolbarButton: public CStationaryToolbarButton {

private:
	CWnd *m_pActionOwner;

public:
	CCommandToolbarButton(); 

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, UINT nBitmapID, UINT nBitmapIndex,
			   CSize m_bitmapSize, UINT nCommand, int nMaxTextChars, DWORD dwButtonStyle = 0, int nMinTextChars = 0);

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText, LPCTSTR pBitmapFile,
			   CSize m_bitmapSize, UINT nCommand, int nMaxTextChars, DWORD dwButtonStyle = 0, int nMinTextChars = 0);

	virtual void OnAction(void);

	virtual void FillInOleDataSource(COleDataSource *pDataSource);
	//Actions will be sent to this owner.  If this function is not used then the
	//message will go to the owner frame.
	virtual void SetActionMessageOwner(CWnd *pActionOwner);
};

void WFE_ParseButtonString(UINT nID, CString &statusStr, CString &toolTipStr, CString &textStr);
#endif
