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

// RDF Tree View for Aurora.  Created by Dave Hyatt.

#ifndef __RDFLINER_H
#define __RDFLINER_H

#include "outliner.h"
#include "property.h"
#include "cxstubs.h"
#include "statbar.h"
#include "navfram.h"
#include "htrdf.h"
#include "rdfglobal.h"
#include "rdfacc.h"

// array of indexes for IDB_BOOKMARKS
#define IDX_BOOKMARKOPEN				0
#define IDX_BOOKMARKCLOSED				1
#define IDX_BOOKMARKMENUOPEN			2
#define IDX_BOOKMARKMENUCLOSED			3
#define IDX_BOOKMARKFOLDEROPEN			4
#define IDX_BOOKMARKFOLDERCLOSED		5
#define IDX_BOOKMARKTOOLBARCLOSED		6
#define IDX_BOOKMARKTOOLBAROPEN			7
#define IDX_BOOKMARKFOLDERMENUOPEN		8
#define IDX_BOOKMARKFOLDERMENUCLOSED    9
#define IDX_BOOKMARKMENUTOOLBARCLOSED	10
#define IDX_BOOKMARKMENUTOOLBAROPEN		11
#define IDX_BOOKMARKFOLDERTOOLBARCLOSED	12
#define IDX_BOOKMARKFOLDERTOOLBAROPEN	13
#define IDX_BOOKMARKALLCLOSED			14
#define IDX_BOOKMARKALLOPEN				15
#define IDX_BOOKMARKCHANGED				16
#define IDX_BOOKMARK					17
#define IDX_BOOKMARKUNKNOWN				18

class CRDFOutlinerParent;

class CRDFOutliner : public COutliner, public CCustomImageObject
{
friend class CRDFOutlinerParent;

private:
	HT_Pane m_Pane;		// The pane that owns this view
	HT_View m_View;		// The view as registered in the hypertree
	HT_Resource m_Node;	// The currently acquired node
	int m_nSortType;	// The type of sort (none, ascending, descending)
	int m_nSortColumn;  // The current column used for sort (none, or the UINT of the column)
	UINT m_hEditTimer;	// The Edit Timer (for editing in place)
	UINT m_hSpringloadTimer; // The Springload timer (for popping open springloaded folders on drag)
	UINT m_hDragRectTimer; // Timer for scrolling on a multiple select drag rectangle
	BOOL m_bNeedToClear; // Used to determine if selection needs to be wiped on a mouse up
	BOOL m_bNeedToEdit;  // Used to determine if an edit field needs to be displayed on a node
	BOOL m_bDoubleClick;  // Used to prevent an edit field from popping up on a double click
	BOOL m_bDataSourceInWindow; // TRUE when something is being dragged around inside the window.
								// Use this to keep springloaded folders behaving properly
	CPtrList m_SpringloadStack; // A list of current OPEN springloaded folders (so that we can close
								// them back up again)
	
	DROPEFFECT m_iCurrentDropAction; // Cache the current drop action

	BOOL m_bCheckForDragRect; // Used to distinguish the dragging of an item from the drawing of a drag
							  // rect for multiple selection
	CRect m_LastDragRect;	  // Position of the last drag rect
	CPoint m_MovePoint;		  // Last position of mouse on a move (Used for drag rect stuff)
	int m_nRectYDisplacement; // Used for scrolling the drag rect

	OutlinerAncestorInfo* m_pAncestor; // Used for drawing the pipes in the tree.
	BOOL m_bPaintingFirstObject; // Used to perform an optimization on the line-drawing part of the tree.
	int m_nSizeOfAncestorArray;	// Also used for the optimization.
	
	CRDFOutlinerParent* m_Parent; // A pointer to the parent container.

	int m_nSelectedColumn; // The currently selected column
	CRDFEditWnd* m_EditField; // The current edit window (if one exists).

	int m_iSpringloadSelection; // Cached last drag-selection to determine springload on drag

	// CLIPFORMATS for drag & drop. 
	CLIPFORMAT m_clipFormatArray[5]; // The list of acceptable formats
	CLIPFORMAT m_cfHTNode;		 // Format for HT nodes.
	CLIPFORMAT m_cfBookmark;	 // Bookmark format (title/URL)
	
	CRDFCommandMap m_MenuCommandMap;  // The command mapping for menus

	CNavMenuBar* m_NavMenuBar;	// A pointer to the title nav menu. NULL if no menu button is present.

	// Tree colors, fonts, and backgrounds
	COLORREF m_ForegroundColor;				// The foreground color.  Used for text, separators, etc.
	COLORREF m_BackgroundColor;				// The background color.  For trees with no bg image.  Also
											// displayed until the image loads.
	COLORREF m_SortBackgroundColor;			// The color used to display a highlighted column (that has a sort
											// imposed upon it.
	COLORREF m_SortForegroundColor;			// The color used to display te highlighted text/separators, etc.

	COLORREF m_SelectionForegroundColor;	// Foreground color of the selection
	COLORREF m_SelectionBackgroundColor;	// Background color of the selection

	COLORREF m_DividerColor;				// Color of the dividers drawn between lines
	BOOL	 m_bDrawDividers;				// Whether or not dividers should be drawn

	CString m_BackgroundImageURL;			// The URL of the background image.
	CRDFImage* m_pBackgroundImage;	// The image for the background.

public:
    CRDFOutliner (HT_Pane thePane, HT_View theView, CRDFOutlinerParent* theParent);
	~CRDFOutliner ( );

	// Inspectors
	HT_View GetHTView() { return m_View; } // Return a handle to the hypertree view
	HT_Resource GetAcquiredNode() { return m_Node; } // Return handle to the acquired HT node
	CRDFOutlinerParent* GetRDFParent() { return m_Parent; } // RDFOutlinerParent handle
	UINT GetSelectedColumn() { return m_nSelectedColumn; } // Returns selected column
	int GetSortColumn() { return m_nSortColumn; }
	int GetSortType() { return m_nSortType; }
	
	// Setters
	void SetSortType(int sortType) { m_nSortType = sortType; }
	void SetSortColumn(int sortColumn) { m_nSortColumn = sortColumn; }
    void SetSelectedColumn(int nColumn) { m_nSelectedColumn = nColumn; }
	void SetDockedMenuBar(CNavMenuBar* bar) { m_NavMenuBar = bar; }

// I am explicitly labeling as virtual any functions that have been
// overridden.  That is, unless I state otherwise, all virtual functions
// are overridden versions of COutliner functions.
	
	virtual void InitializeItemHeight(int iDesiredSize) { m_itemHeight = 19; }
		// Overridden to place a pixel of padding on either side of the line and to add a pixel for the
		// divider that is drawn between lines.

	virtual void AdjustTipSize(int& left, int& top, int& hor, int& ver)
	{ top += 1; ver -= 3; };	// Account for the dividers between lines and for the padding.

	virtual int GetIndentationWidth() { return m_pIUserImage->GetImageWidth(); };
		// Gets the width to indent at each level.

	void FocusCheck(CWnd* pWnd, BOOL gotFocus);  
		// Called to potentially update the embedded menu bar in the docked view.

	BOOL IsSelectedColumn(int i) { return m_nSelectedColumn == i; }

	virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );  
		// Grab the text for a particular column.  

	virtual void * AcquireLineData ( int iLine );
		// Acquire (nice spelling, heh) the node for a given line.

	virtual void ReleaseLineData ( void * pLineData );
		// Release the line data for a given line.

	virtual void GetTreeInfo( int iLine, uint32 * pFlags, int * iDepth, 
					  OutlinerAncestorInfo ** pAncestor );
		// Get information about the node at iLine.

	virtual HFONT GetLineFont ( void * pLineData );
		// Get the font (e.g., regular, italic, bold) for the given line.

	virtual int TranslateIcon ( void * pData );
		// Figures out which icon should be used.

	virtual int TranslateIconFolder( void *pData );
		// Don't have a damn clue what this does.

	virtual int ToggleExpansion(int iLine);
		// Called to expand a node.
	
	void FinishExpansion(int parentIndex);
		// Called by the notifyProc to complete the expansion

	virtual void SelectItem(int iSel, int mode, UINT flags);
		// Called to select an item... handles single/double click...everything...

	void OnSelDblClk(int iLine);
		// Different from base class.  Line info is passed as arg.

	virtual void PropertyMenu(int iLine, UINT flags);
		// Displays the context-sensitive menu (when the user right-clicks on a node)

	virtual BOOL IsSelected(int iLine);
		// Used by base class to determine if a line is selected.  Function
		// queries RDF backend instead of relying on the m_Selection member
		// variable in COutliner.

	void EditTextChanged(char* text);
		// Function is called when a column's data has been edited in place.  This
		// function talks to the back end and makes sure the node gets changed.

	char* GetTextEditText();
		// Copies the edit field's text to a char*.  The text must be deleted by the
		// caller of this function.

	void AddTextEdit();
		// Creates a text edit on the selected item and column.

	void RemoveTextEdit();
		// Kills the text edit control.

	void MoveToNextColumn();
		// Called when the user tabs in the edit field to move to the next column.

	CRect GetColumnRect(int lineNo, int columnNo);
		// Gets the rect associated with a specific column and line

	virtual void ColumnsSwapped();
		// Called when the columns' positions have changed (e.g., the user dragged one column to the
		// left of another column).  Added this function so that RDFOutliner (unlike Outliner) will
		// force the tree images to remain on the left.

	int DetermineClickLocation(CPoint point);
		// The master click function.  Determines whether the user clicked on a trigger, on the item itelf,
		// or on the background.

	virtual BOOL TestRowCol(POINT point, int &, int &); 
		// Overridden because Outliner doesn't set the column hit if the row doesn't contain data.

	void copyAncestorValues(HT_Resource r, int numItems, OutlinerAncestorInfo* oldInfo, OutlinerAncestorInfo* newInfo);
		// Used for drawing the lines in the tree.

	void HandleEvent(HT_Notification ns, HT_Resource n, HT_Event whatHappened);
		// The function which handles all the events which occur in the view.
	
	void DisplayURL();
		// Called to display a URL for a non-container node.
	
	BOOL IsDocked();
		// True if the RDF view is docked.

	virtual void LoadComplete(HT_Resource pResource); 
		// Overridden from CCustomObject.  Called when a custom URL image has finished loading.

	virtual BOOL DeleteItem(int iSel);
		// Called when the user decides to delete an item.  The selection argument is completely
		// ignored. (We rely on HT's selection when performing the deletion.)

	virtual int DrawPipes ( int iLineNo, int iColNo, int offset, HDC hdc, void * pLineData );
		// Overridden to handle the drawing of custom icons.

	void PaintLine ( int iLineNo, HDC hdc, LPRECT lpPaintRect, HBRUSH hHighlightBrush,
						       HPEN hHighlightPen );
		// This PaintLine is used instead of the one in the outliner base class. Overridden so
		// that individual columns can be selected (instead of an entire line).

	void PaintColumn(int iLineNo, int iColumn, LPRECT lpColumnRect, 
							   HDC hdc, void * pLineData, HBRUSH hHighlightBrush,
							   HPEN hHighlightPen);
		// Used instead of the outliner PaintColumn.  Same reason as above.

	void DrawColumn(HDC hdc, LPRECT lpColumnRect, LPCTSTR lpszString,
					CropType_t cropping, AlignType_t alignment, HBRUSH theBrush = NULL, 
					BOOL hasFocus = FALSE);
		// Used instead of the outliner DrawColumnText.  Same reason.

	void InvalidateIconForResource(HT_Resource r);
		// Invalidates only the icon for a given node.  The rest of the line is left alone.

	CRect ConstructDragRect(const CPoint& pt1, const CPoint& pt2);
		// Constructs a new rectangle with the two points specified as opposing corners.

	void EraseDragRect(CDC* pDC, CRect rect);
		// Helper to blow away the drag rect

	void DragRectScroll(BOOL bBackwards);
		// Function to scroll the drag rect

	virtual void InitializeClipFormats();
		// Registers the formats we accept (drag/drop) when the window is created.

	virtual CLIPFORMAT * GetClipFormatList();
		// Returns an array of CLIPFORMATS that we are willing to handle.  Used
		// for drag over feedback.

	virtual DROPEFFECT DropSelect (int iLineNo, COleDataObject *object );
		// Used to determine type of drop.

	virtual int GetDragHeartbeat();
		// Speed at which the tree scrolls on a drag.  Overridden to be faster than COutliner.

	virtual void AcceptDrop( int iLineNo, COleDataObject *pDataObject, DROPEFFECT dropEffect );
		// Called when a drop occurs on a given line in the view.  This function actually
		// handles the drop.

	virtual BOOL HighlightIfDragging(void);
		// Called to determine whether or not a node should be highlighted when an item is dragged
		// OVER it.  We only highlight containers (folders).

	virtual void PaintDragLine(HDC hdc, CRect &rectColumn);
		// Called to draw the drag line that indicates the drop will occur AFTER this item in the tree.

	virtual COleDataSource * GetDataSource();
		// Called when a drag is initiated in this view to copy the node.

	COleDropTarget* GetDropTarget() { return (COleDropTarget*)m_pDropTarget; }
		// Returns the drop target associated with the COutliner base class

	virtual COutlinerDropTarget* CreateDropTarget();
		// Called to create the RDF drop target.

	afx_msg int OnCreate ( LPCREATESTRUCT );
	    // Overridden to allow for special create stuff.

	afx_msg void OnPaint();
		// Overridden to perform a drawing optimization (not performed by Outliner)

	afx_msg void OnLButtonDown ( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp (UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDown ( UINT nFlags, CPoint point );
	afx_msg void OnRButtonUp ( UINT nFlags, CPoint point );
		// All overridden to allow the drawing of a drag rectangle for multiple selection
		// and to implement improved hit testing.

	afx_msg void OnTimer(UINT id);
		// Used for editing columns and what-not.

	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
		// Overridden to update the nav title caption colors (in a docked Aurora view).


	afx_msg BOOL OnCommand(UINT wParam, LONG lParam);
		// Menu commands are handled by the rdfliner.

	DECLARE_MESSAGE_MAP();

	friend class CRDFDropTarget;
	friend class CRDFOutlinerParent;
};

class CRDFDropTarget: public COutlinerDropTarget
{
public:
    DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
    BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
    DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
    void OnDragLeave(CWnd* pWnd);

	CRDFDropTarget(COutliner* pOutliner) :COutlinerDropTarget(pOutliner) {};
};

class CRDFOutlinerParent : public COutlinerParent, CCustomImageObject 
{
private:
	CRDFCommandMap columnMap;
	COLORREF m_ForegroundColor;
	COLORREF m_BackgroundColor;
	CRDFImage* m_pBackgroundImage;
	CString m_BackgroundImageURL;

public:
	CRDFOutlinerParent(HT_Pane thePane, HT_View theView);

	BOOL PreCreateWindow(CREATESTRUCT& cs);
	COutliner* GetOutliner();
	void CreateColumns();
	BOOL RenderData( int iColumn, CRect & rect, CDC &dc, LPCTSTR text );
	BOOL ColumnCommand( int iColumn );
	void Initialize();
	CRDFCommandMap& GetColumnCommandMap() { return columnMap; }

	void LoadComplete(HT_Resource r) { Invalidate(); }

protected:
    afx_msg void OnDestroy();
	afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
};


class CRDFContentView : public CContentView
{
public:
    COutlinerParent * m_pOutlinerParent;

// Construction
public:
	CRDFContentView(CRDFOutlinerParent* outlinerStuff)
	{ m_pOutlinerParent = outlinerStuff; };

	~CRDFContentView() 
	{
		delete m_pOutlinerParent;
	}

	COutlinerParent* GetOutlinerParent() { return m_pOutlinerParent; }

// This functionality has been folded in from COutlinerView. I no longer derive from this class
// but instead come off of CContentView.

    void CreateColumns ( )
    {
        m_pOutlinerParent->CreateColumns ( );
    }

	void InvalidateOutlinerParent();

	static void DisplayRDFTree(CWnd* pParent, int width, int height, RDF_Resource rdfResource);
		// This function can be called to create an embedded RDF tree view inside another window.
		// Used to embed the tree in HTML.

protected:
    virtual void OnDraw(CDC *pDC);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    DECLARE_DYNCREATE(CRDFContentView)
	//{{AFX_MSG(CMainFrame)
	afx_msg LRESULT OnNavCenterQueryPosition(WPARAM wParam, LPARAM lParam);
	afx_msg int OnCreate ( LPCREATESTRUCT );
    afx_msg void OnSize ( UINT, int, int );
    afx_msg void OnSetFocus ( CWnd * pOldWnd );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class FAR CExportRDF : public CDialog
{
protected:
	CString m_csCaption;

// Construction
public:
    CExportRDF(CWnd* pParent = NULL);  // standard constructor

    void SetSecureTitle( CString &csTitle ) { m_csTitle = csTitle; }
    
// Dialog Data
    //{{AFX_DATA(CExportRDF)
    enum { IDD = IDD_EXPORTRDF };
    CString m_csAsk;
    CString m_csAns;
    //}}AFX_DATA

    CString m_csTitle;
    
// Implementation
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    DECLARE_MESSAGE_MAP()
};


// Functions that will be used by the Personal Toolbar and Quickfile.  Allow the
// display of arbitrary icons (or local file system icons)

#endif
