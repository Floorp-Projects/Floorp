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

#ifndef _USERTLBR_H
#define _USERTLBR_H

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
#include "toolbar2.h"
#include "rdfacc.h"
#include "cxicon.h"

#define CRDFToolbarButtonDropTargetBase	CToolbarButtonDropTarget

class CRDFToolbarButtonDropTarget : public CRDFToolbarButtonDropTargetBase
{
	public:
		CRDFToolbarButtonDropTarget(){m_pButton = NULL;}
	protected:
		virtual DROPEFFECT ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point);

}; 

#define CRDFToolbarButtonBase CDragableToolbarButton

class CRDFToolbarButton: public CRDFToolbarButtonBase, public CCustomImageObject 
{
protected:
	HT_Resource m_Node;				// The resource corresponding to the RDF node
	BOOKMARKITEM m_bookmark;
	UINT m_uCurBMID;
    CMapWordToPtr m_BMMenuMap;		// Maps the bookmark menu IDs
    int currentRow;                 // The row of the personal toolbar this button resides on.
	CDropMenu* m_pCachedDropMenu;	// A pointer to a drop menu that is tracked across 
									// node opening (HT) callbacks
	CNSNavFrame* m_pTreeView;		// A pointer to a tree popup that is tracked.
	BOOL m_bShouldShowRMMenu;		// Set to TRUE by default.  Quickfile/Breadcrumbs set it to FALSE.

	CRDFCommandMap m_MenuCommandMap;	// Command map for back-end generated right mouse menu commands.
	
	int m_nActualBitmapHeight;		// The actual bitmap's height.
	
	CString m_BorderStyle;			// Solid, beveled, or none.

public:
	CRDFToolbarButton();
	~CRDFToolbarButton();

	virtual int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText,
			   CSize bitmapSize, int nMaxTextChars, int nMinTextChars, BOOKMARKITEM bookmark, 
			   HT_Resource pNode, DWORD dwButtonStyle = 0);

    int GetRow() { return currentRow; }
    void SetRow(int i) { currentRow = i; }

	virtual BOOL foundOnRDFToolbar() { return TRUE; } // RDF Buttons must ALWAYS reside on RDF toolbars.  (Derived
											  // classes could have different behavior, but base-class buttons
											  // make the assumption that they sit on an RDF toolbar.

	CNSNavFrame* GetTreeView() { return m_pTreeView; }

	virtual void OnAction(void);
	virtual CSize GetButtonSizeFromChars(CString s, int c);
    virtual CSize GetMinimalButtonSize();
    virtual CSize GetMaximalButtonSize();

	virtual BOOL IsSpring() { return FALSE; } // Whether or not the button will expand to consume
											  // all remaining space on a row.

	virtual BOOL UseLargeIcons() { return FALSE; }
	virtual void UpdateIconInfo();
	HT_IconType TranslateButtonState();

	virtual void DrawButtonBitmap(HDC hDC, CRect rcImg);

	virtual void FillInOleDataSource(COleDataSource *pDataSource);

	BOOKMARKITEM GetBookmarkItem();
	HT_Resource GetNode(void) { return m_Node;}
	void SetNode(HT_Resource pBookmark, BOOL bAddRef = TRUE);
	void SetBookmarkItem(BOOKMARKITEM bookmark);
    virtual void EditTextChanged(char *pText);
    void SetTextWithoutResize(CString text);

	virtual UINT GetBitmapID(void);
	virtual UINT GetBitmapIndex(void);

	void FillInMenu(HT_Resource pNode);
	void LoadComplete(HT_Resource r);
	virtual void DrawCustomIcon(HDC hDC, int x, int y);
	virtual void DrawLocalIcon(HDC hDC, int x, int y);
	virtual void DrawUpButton(HDC hDC, CRect & rect);
	virtual void DrawDownButton(HDC hDC, CRect & rect);
	virtual void DrawCheckedButton(HDC hDC, CRect & rect);

	virtual HT_View GetHTView() { return HT_GetView(m_Node); }

	virtual BOOL NeedsUpdate();

	// button mode is defined solely in the RDF
	virtual void SetPicturesOnly(int bPicturesOnly);
	virtual void SetButtonMode(int nToolbarStyle);
	int          GetButtonMode(void);

protected:
	virtual void DrawPicturesAndTextMode(HDC hDC, CRect rect);
    virtual BOOL CreateRightMouseMenu(void);
	virtual CWnd* GetMenuParent(void);
	virtual void GetPicturesAndTextModeTextRect(CRect &rect);
	virtual void GetPicturesModeTextRect(CRect &rect);
    virtual void DrawButtonText(HDC hDC, CRect rcTxt, CSize sizeTxt, CString strTxt);
	

	virtual HFONT GetFont(HDC hDC);
	virtual int DrawText(HDC hDC, LPCSTR lpString, int nCount, LPRECT lpRect, UINT uFormat);
	virtual BOOL GetTextExtentPoint32(HDC hDC, LPCSTR lpString, int nCount, LPSIZE lpSize);

protected:
		// Generated message map functions
	//{{AFX_MSG(CRDFToolbarButton)
	afx_msg LRESULT OnDragMenuOpen(WPARAM, LPARAM);
    afx_msg BOOL OnCommand(UINT wParam,LONG lParam);
	afx_msg LRESULT OnDropMenuDropOccurred(WPARAM, LPARAM);
	afx_msg LRESULT OnDropMenuDraggingOccurred(WPARAM, LPARAM);
	afx_msg LRESULT OnDropMenuClosed(WPARAM, LPARAM);
	afx_msg LRESULT OnFillInMenu(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSysColorChange( );
	afx_msg void OnPaint();
	afx_msg int OnMouseActivate( CWnd* pDesktopWnd, UINT nHitTest, UINT message );
	
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()


};

void toolbarNotifyProcedure (HT_Notification ns, HT_Resource n, HT_Event whatHappened);

/****************************************************************************
* 
* Class: CRDFSeparatorButton
*
****************************************************************************/

class CRDFSeparatorButton : public CRDFToolbarButton
{
protected:
	
public:
	virtual CSize GetButtonSizeFromChars(CString s, int c);
		// Overridden to handle special width/height requirements.

	virtual void DrawButtonBitmap(HDC hDC, CRect rcImg);
	virtual void DrawButtonText(HDC hDC, CRect rcTxt, CSize sizeTxt, CString strTxt) {};

	// Generated message map functions
	//{{AFX_MSG(CRDFSeparatorButton)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/****************************************************************************
*
*	Class: CRDFToolbarDropTarget
*
*	DESCRIPTION:
*		This class provides a drag-drop target for the bookmark quick file
*		button.
*
****************************************************************************/
class CRDFToolbar;

class CRDFToolbarDropTarget : public COleDropTarget
{
private:
	CRDFToolbar *m_pToolbar;

public:
	CRDFToolbarDropTarget() {m_pToolbar = NULL;}
	void Toolbar(CRDFToolbar *pToolbar);
	 
protected:

	virtual DROPEFFECT OnDragEnter(CWnd * pWnd,
		COleDataObject * pDataObject, DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(CWnd * pWnd,
		COleDataObject * pDataObject, DWORD dwKeyState, CPoint point );
	virtual BOOL OnDrop(CWnd * pWnd, COleDataObject * pDataObject,
		DROPEFFECT dropEffect, CPoint point);

	private:

};

class CRDFToolbar : public CNSToolbar2, public CCustomImageObject {

private:
	CRDFToolbarDropTarget m_DropTarget;
	HT_View m_ToolbarView;
	CRDFCommandMap m_MenuCommandMap;	// Command map for back-end generated right mouse menu commands.
	
    int m_nNumberOfRows;
	int m_nRowHeight;

	CRDFToolbarButton* m_pDragButton;
	int m_iDragFraction;

	COLORREF m_BackgroundColor;
	COLORREF m_ForegroundColor;
	COLORREF m_RolloverColor;
	COLORREF m_PressedColor;
	COLORREF m_DisabledColor;
	COLORREF m_ShadowColor;
	COLORREF m_HighlightColor;

	CRDFImage* m_pBackgroundImage;

	static int m_nMinToolbarButtonChars;
	static int m_nMaxToolbarButtonChars;
	
public:

	// toolbar style descriptors for translating between our superclass' idea of style
	// and HT's idea, an intersection occupied by this class
	// (see HTDescriptorFromStyle()).
	enum StyleType {
		nPicAndTextStyle = 0,
		nPicStyle,
		nTextStyle
	};

	CRDFToolbar(HT_View theView, int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, int nPicturesHeight,
				 int nTextHeight);
	~CRDFToolbar();

	// Used to create toolbars
	static CRDFToolbar* CreateUserToolbar(HT_View theView, CWnd* pParent);

	int Create(CWnd *pParent);
	virtual int GetHeight(void);
    virtual int GetRowWidth();

    // Override layout scheme to dynamically resize buttons.
    void LayoutButtons(int nIndex); // Index will be ignored by this version of the function
                                    // since adding/deleting buttons may cause all buttons to resize
	
	// Toolbar's event handler.
	void HandleEvent(HT_Notification ns, HT_Resource n, HT_Event whatHappened);

	void AddHTButton(HT_Resource n);  // Called to add a new button to the toolbar

    void SetMinimumRows(int rowWidth); // Called to determine and set the # of rows required by the toolbar.

    int GetRows() { return m_nNumberOfRows; }
    void SetRows(int i) { m_nNumberOfRows = i; }

    void WidthChanged(int width);  // Inherited from toolbar.  Called when width changes, e.g. window resize or animation placed on toolbar.

	void FillInToolbar(); // Called to create and place the buttons on the toolbar
	int GetDisplayMode(void);

	HT_View GetHTView() { return m_ToolbarView; }  // Returns the HT-View for this toolbar.
	void SetHTView(HT_View v) { m_ToolbarView = v; }

	void SetDragFraction(int i) { m_iDragFraction = i; }
	int GetDragFraction() { return m_iDragFraction; }

	void SetDragButton(CRDFToolbarButton* pButton) { m_pDragButton = pButton; }
	CRDFToolbarButton* GetDragButton() { return m_pDragButton; }

	void SetRowHeight(int i) { m_nRowHeight = i; }
	int GetRowHeight() { return m_nRowHeight; }

	// Color/background customizability stuff
	
	COLORREF GetBackgroundColor() { return m_BackgroundColor; }
	COLORREF GetForegroundColor() { return m_ForegroundColor; }
	COLORREF GetRolloverColor() { return m_RolloverColor; }
	COLORREF GetPressedColor() { return m_PressedColor; }
	COLORREF GetDisabledColor() { return m_DisabledColor; }
	COLORREF GetShadowColor() { return m_ShadowColor; }
	COLORREF GetHighlightColor() { return m_HighlightColor; }

	void ComputeColorsForSeparators();

	CRDFImage* GetBackgroundImage() { return m_pBackgroundImage; }
	void SetBackgroundColor(COLORREF c) { m_BackgroundColor = c; }
	void SetForegroundColor(COLORREF c) { m_ForegroundColor = c; }
	void SetRolloverColor(COLORREF c) { m_RolloverColor = c; }
	void SetPressedColor(COLORREF c) { m_PressedColor = c; }
	void SetDisabledColor(COLORREF c) { m_DisabledColor = c; }
	void SetShadowColor(COLORREF c) { m_ShadowColor = c; }
	void SetHighlightColor(COLORREF c) { m_HighlightColor = c; }

	void SetBackgroundImage(CRDFImage* p) { m_pBackgroundImage = p; }

	void LoadComplete(HT_Resource r);

	void ChangeButtonSizes(void); // Overridden to prevent separators and url bars from changing size.

	virtual void SetToolbarStyle(int nToolbarStyle);

	// returns the HT string descriptor corresponding to the toolbar style.
	static const char * HTDescriptorFromStyle(int style);
	// returns the toolbar style corresponding to the HT string descriptor.
	// returns < 0 if no corresponding style is found
	static int StyleFromHTDescriptor(const char *descriptor);

protected:
    // Helper function used in conjunction with LayoutButtons
    void ComputeLayoutInfo(CRDFToolbarButton* pButton, int numChars, int rowWidth, int& usedSpace);

	// Generated message map functions
	//{{AFX_MSG(CRDFToolbar)
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	afx_msg void OnPaint(void);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};

class CRDFDragToolbar : public CDragToolbar
{
public:
	
	virtual int Create(CWnd *pParent, CToolbarWindow *pToolbar);
	virtual void SetOpen(BOOL bIsOpen);
	virtual void BeActiveToolbar();
	virtual BOOL MakeVisible(BOOL visible);
	virtual BOOL WantsToBeVisible();

protected:
	void CopySettingsToRDF(void);

public:
	// Generated message map functions
	//{{AFX_MSG(CDragToolbar)
	afx_msg void OnPaint(void);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

class CRDFToolbarHolder : public CCustToolbar
{
	DECLARE_DYNAMIC(CRDFToolbarHolder)

protected:
	HT_Pane m_ToolbarPane;
	CFrameWnd* m_pCachedParentWindow;
	CRDFToolbarButton* m_pCurrentDockedButton;
	CRDFToolbarButton* m_pCurrentPopupButton;

public:
	CRDFToolbarHolder(int maxToolbars, CFrameWnd* pParent);
	virtual ~CRDFToolbarHolder();

	CRDFToolbarButton* GetCurrentButton(int state) 
	{ 
		if (state == HT_POPUP_WINDOW)
			return m_pCurrentPopupButton;
		else return m_pCurrentDockedButton;
	}

	void SetCurrentButton(CRDFToolbarButton* button, int state) 
	{ 
		if (state == HT_POPUP_WINDOW)
			m_pCurrentPopupButton = button;
		else m_pCurrentDockedButton = button;
	}

	HT_Pane GetHTPane() { return m_ToolbarPane; }
	void SetHTPane(HT_Pane p) { m_ToolbarPane = p; }
	CFrameWnd* GetCachedParentWindow() { return m_pCachedParentWindow; }
	void InitializeRDFData();

	virtual CDragToolbar* CreateDragBar()
	{
		return new CRDFDragToolbar();
	}

protected:
	virtual void DrawSeparator(int i, HDC hDC, int nStartX, int nEndX, int nStartY, BOOL bToolbarSeparator = TRUE);
	
};

#endif
