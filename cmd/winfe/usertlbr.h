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

class CRDFToolbarButton: public CRDFToolbarButtonBase, public CCustomImageObject {

protected:
	HT_Resource m_Node;				// The resource corresponding to the RDF node
	BOOKMARKITEM m_bookmark;
	UINT m_uCurBMID;
    CMapWordToPtr m_BMMenuMap;		// Maps the bookmark menu IDs
    int currentRow;                 // The row of the personal toolbar this button resides on.
	CDropMenu* m_pCachedDropMenu;	// A pointer to a drop menu that is tracked across 
									// node opening (HT) callbacks
	BOOL m_bShouldShowRMMenu;		// Set to TRUE by default.  Quickfile/Breadcrumbs set it to FALSE.

	CRDFCommandMap m_MenuCommandMap;	// Command map for back-end generated right mouse menu commands.

public:
	CRDFToolbarButton();
	~CRDFToolbarButton();

	int Create(CWnd *pParent, int nToolbarStyle, CSize noviceButtonSize, CSize advancedButtonSize,
			   LPCTSTR pButtonText, LPCTSTR pToolTipText, 
			   LPCTSTR pStatusText,
			   CSize bitmapSize, int nMaxTextChars, int nMinTextChars, BOOKMARKITEM bookmark, 
			   HT_Resource pNode, DWORD dwButtonStyle = 0);

    int GetRow() { return currentRow; }
    void SetRow(int i) { currentRow = i; }

	virtual void OnAction(void);
	virtual CSize GetButtonSizeFromChars(CString s, int c);
    virtual CSize GetMinimalButtonSize();
    virtual CSize GetMaximalButtonSize();

	virtual BOOL UseLargeIcons() { return FALSE; }
	virtual void UpdateIconInfo() { DetermineIconType(m_Node, UseLargeIcons()); }

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
	
	virtual HT_View GetHTView() { return HT_GetView(m_Node); }

protected:
	virtual void DrawPicturesMode(HDC hDC, CRect rect);
	virtual void DrawPicturesAndTextMode(HDC hDC, CRect rect);
    virtual BOOL CreateRightMouseMenu(void);
	virtual CWnd* GetMenuParent(void);
	virtual void GetPicturesAndTextModeTextRect(CRect &rect);
	virtual void GetPicturesModeTextRect(CRect &rect);
    virtual void DrawButtonText(HDC hDC, CRect rcTxt, CSize sizeTxt, CString strTxt);
	
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

	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()


};

void ptNotifyProcedure (HT_Notification ns, HT_Resource n, HT_Event whatHappened);

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

class CRDFToolbar : public CNSToolbar2 {

private:
	CRDFToolbarDropTarget m_DropTarget;
	HT_Pane m_PersonalToolbarPane;
	CRDFCommandMap m_MenuCommandMap;	// Command map for back-end generated right mouse menu commands.
	
    int m_nNumberOfRows;

	CRDFToolbarButton* m_pDragButton;
	int m_iDragFraction;

	static int m_nMinToolbarButtonChars;
	static int m_nMaxToolbarButtonChars;

public:
	CRDFToolbar(int nMaxButtons, int nToolbarStyle, int nPicturesAndTextHeight, int nPicturesHeight,
				 int nTextHeight);
	~CRDFToolbar();

	// Used to create toolbars
	static CRDFToolbar* CreateUserToolbar(CWnd* pParent);

	int Create(CWnd *pParent);
	virtual int GetHeight(void);
    
    // Override layout scheme to dynamically resize buttons.
    void LayoutButtons(int nIndex); // Index will be ignored by this version of the function
                                    // since adding/deleting buttons may cause all buttons to resize
	
	
	void AddHTButton(HT_Resource n);  // Called to add a new button to the toolbar

    void SetMinimumRows(int rowWidth); // Called to determine and set the # of rows required by the toolbar.

    int GetRows() { return m_nNumberOfRows; }
    void SetRows(int i) { m_nNumberOfRows = i; }

    void WidthChanged(int width);  // Inherited from toolbar.  Called when width changes, e.g. window resize or animation placed on toolbar.

	void FillInToolbar(); // Called to create and place the buttons on the toolbar

	HT_Pane GetPane() { return m_PersonalToolbarPane; }  // Returns the HT-Pane

	void SetDragFraction(int i) { m_iDragFraction = i; }
	int GetDragFraction() { return m_iDragFraction; }

	void SetDragButton(CRDFToolbarButton* pButton) { m_pDragButton = pButton; }
	CRDFToolbarButton* GetDragButton() { return m_pDragButton; }

protected:
    // Helper function used in conjunction with LayoutButtons
    void ComputeLayoutInfo(CRDFToolbarButton* pButton, int numChars, int rowWidth, int& usedSpace);

	// Generated message map functions
	//{{AFX_MSG(CRDFToolbar)
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnCommand( WPARAM wParam, LPARAM lParam );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};


#endif
