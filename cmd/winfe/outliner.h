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

#ifndef _OUTLINER_H
#define _OUTLINER_H

#include "apiimg.h"
#include "apioutln.h"
#include "msgcom.h"
#include "tooltip.h"

#define WM_COLUMN_COMMAND   WM_USER+5000

#define OUTLINER_OPENFOLDER     1
#define OUTLINER_CLOSEDFOLDER   2
#define OUTLINER_ITEM           3

// array of indexes for IDB_OUTLINER
#define IDX_TREEITEM            0
#define IDX_TREEFOLDERCLOSED    1
#define IDX_TREEFOLDEROPEN      2
#define IDX_CLOSEDBOTTOMPARENT  3
#define IDX_CLOSEDTOPPARENT     4
#define IDX_CLOSEDMIDDLEPARENT  5
#define IDX_CLOSEDSINGLEPARENT  6
#define IDX_OPENBOTTOMPARENT    7
#define IDX_OPENMIDDLEPARENT    8
#define IDX_OPENTOPPARENT       9
#define IDX_OPENSINGLEPARENT    10
#define IDX_BOTTOMITEM          11
#define IDX_MIDDLEITEM          12
#define IDX_TOPITEM             13
#define IDX_VERTPIPE            14
#define IDX_HORZPIPE            15
#define IDX_EMPTYITEM           16

// array of indexes for IDB_COLUMN
#define IDX_SORTINDICATORUP     0
#define IDX_SORTINDICATORDOWN   1
#define IDX_PUSHLEFT            2
#define IDX_PUSHRIGHT           3
#define IDX_PUSHLEFTI	        4
#define IDX_PUSHRIGHTI          5

#ifndef FLOAT
 #define FLOAT float
#endif

typedef struct 
{
    XP_Bool has_next;
    XP_Bool has_prev;
} OutlinerAncestorInfo;

typedef struct {
    LPCTSTR pHeader;
    Column_t cType;
    int iMinColSize;
    int iMaxColSize;
    FLOAT fPercent;
    FLOAT fDesiredPercent;    
    int iCol;
    UINT iCommand;
    BOOL bDepressed;
	BOOL bVisible;
    BOOL bIsButton;
    CropType_t cropping;
	AlignType_t alignment;
} OutlinerColumn_t;

class CTip; // In tip.h

class COutliner : public CWnd,
                  public CGenericObject,
                  public IOutliner
{
    friend class COutlinerDropTarget;
	friend class COutlinerParent;

#ifdef _WIN32
private:
    //  MouseWheel deltra tracker.
    int m_iWheelDelta;
#endif

public:
	OutlinerColumn_t ** m_pColumn;

	int m_iNumColumns;
	int m_iVisColumns;

protected:
    LPUNKNOWN m_pUnkImage;
    LPIMAGEMAP m_pIImage;
    LPUNKNOWN m_pUnkUserImage;
    LPIMAGEMAP m_pIUserImage;

    int m_iTotalLines;
    int m_iTopLine;
    int m_cyChar, m_cxChar;
    int m_itemHeight;
    int m_iPaintLines;
	UINT m_idImageCol;
	BOOL m_bHasPipes;

	int m_iCSID;

	static BOOL m_bTipsEnabled;
	CTip *m_pTip;
	int m_iTipState, m_iTipTimer; 
	int m_iTipRow, m_iTipCol;

    COutlinerDropTarget * m_pDropTarget;

    HFONT m_hBoldFont, m_hRegFont, m_hItalFont;

    BOOL m_bDragging;
    RECT m_rcHit;
	POINT m_ptHit;
	int m_iColHit, m_iRowHit;

	int m_iSelection, m_iFocus;
    int m_iLastSelected;
    int m_iDragSelection;
	int m_iDragSelectionLineHalf;			// Which half of the selection is the drag over
	int m_iDragSelectionLineThird;			// Which third of the selection is the drag over
  	BOOL m_bDragSectionChanged;

	BOOL m_bClearOnRelease;
	BOOL m_bSelectOnRelease;

	int m_iTotalWidth;

    BOOL m_bDraggingData;
    
// Methods

	void TipHide ( );
	void HandleMouseMove( POINT point );
    int LineFromPoint (POINT point);
	virtual BOOL TestRowCol(POINT point, int &, int &); 

	virtual UINT GetOutlinerBitmap(void);
    BOOL ViewerHasFocus ( );
	virtual void ColumnsSwapped() {};
	void DoToggleExpansion( int iLine );
	void DoExpand( int iLine );
	int DoExpandAll( int iLine );
	void DoCollapse( int iLine );
	int DoCollapseAll( int iLine );
    int GetPipeIndex ( void *pData, int iDepth, OutlinerAncestorInfo * pAncestor );
    void RectFromLine ( int iLineNo, LPRECT lpRect, LPRECT lpOutRect );
    void EraseLine ( int iLineNo, HDC hdc, LPRECT lpRect );
    virtual int DrawPipes ( int iLineNo, int iColNo, int offset, HDC hdc, void * pLineData );
    void PaintLine ( int iLineNo, HDC hdc, LPRECT lpPaintRect );
    void PaintColumn ( int iLineNo, int iColumn, LPRECT lpColumnRect, HDC hdc, 
					   void * pLineData );
	virtual void PaintDragLine(HDC hdc, CRect &rectColumn);
    void EnableScrollBars ( void );
    void DrawColumnText (HDC hdc, LPRECT lpColumnRect, LPCTSTR lpszText, 
						 CropType_t cropping, AlignType_t alignment );

// Basic Overrideables
	virtual void AdjustTipSize(int& left, int& top, int& hor, int& vert) {};
	virtual int GetIndentationWidth();

	virtual COutlinerDropTarget* CreateDropTarget();

    // right mouse menu stuff
    virtual void PropertyMenu(int iSel, UINT flags=0);

	// Line stuff
	virtual int GetDepth( int iLine );
	virtual int GetNumChildren( int iLine );
	virtual int GetParentIndex( int iLine );
	virtual BOOL IsCollapsed( int iLine );
    virtual BOOL HasFocus ( int iLine );
    virtual BOOL IsSelected ( int iLine );

    virtual int ToggleExpansion ( int iLine );
    virtual int Expand ( int iLine );
    virtual int Collapse ( int iLine );
    virtual int ExpandAll ( int iLine );
    virtual int CollapseAll ( int iLine );

	void InvalidateLine ( int iLineNo );
	void InvalidateLines( int iStart, int iCount );

	virtual BOOL HighlightIfDragging(void);

	// Column Stuff
	void GetColumnRect( int iCol, RECT &rc );
	void InvalidateColumn( int iCol );

	// Drawing stuff
    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual LPCTSTR GetColumnTip ( UINT iColumn, void * pLineData );
	virtual BOOL RenderData ( UINT, CRect &, CDC &, LPCTSTR lpsz = NULL );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual HFONT GetLineFont ( void * pLineData );
    virtual int TranslateIcon ( void * );
    virtual int TranslateIconFolder ( void * );

	// Navigation stuff
    virtual void PositionHome ( void );
    virtual void PositionEnd ( void );
    virtual void PositionPageUp ( void );
    virtual void PositionPageDown ( void );
    virtual void PositionPrevious ( void );
    virtual void PositionNext ( void );

    // Sizing stuff
    virtual BOOL SqueezeColumns( int iColFrom = -1, int iDelta = 0, BOOL bRepaint = TRUE );
    virtual void InitializeItemHeight(int iDesiredSize);

	// Command stuff
    virtual BOOL ColumnCommand ( int idColumn, int iLineNo );

    // drag drop stuff

	// clip format
    virtual void InitializeClipFormats(void);
    virtual CLIPFORMAT * GetClipFormatList(void);
    virtual BOOL RecognizedFormat( COleDataObject * );

	// initiating drag
    virtual COleDataSource * GetDataSource(void);
    virtual void InitiateDragDrop(void);

	// accepting drop
    virtual DROPEFFECT DropSelect(int iLineNo, COleDataObject *pObject);
    virtual void AcceptDrop( int iLineNo, COleDataObject * pObject, DROPEFFECT dropEffect );
    virtual void EndDropSelect (void);

	// accessors
    virtual int GetDropLine(void);
	virtual int GetDragHeartbeat();

    BOOL IsDragging() { return m_bDragging; }
	
	virtual void OnSelChanged();
	virtual void OnSelDblClk();

public:
    COutliner (BOOL bUseTriggerAndLineBitmaps = TRUE);
    ~COutliner ( );

	STDMETHODIMP QueryInterface(REFIID,LPVOID *);
   
	void EnableTips(BOOL = TRUE);
	BOOL GetTipsEnabled() { return m_bTipsEnabled; }

	void SetCSID( int csid );
	int GetCSID() { return m_iCSID; }

	int AddColumn ( LPCTSTR header, UINT idCol, 
		int iMinCol, int iMaxCol = 10000, 
		Column_t cType = ColumnFixed, int iPercent = 50,
		BOOL bIsButton = TRUE, CropType_t ct = CropRight, 
		AlignType_t at = AlignLeft );

	int GetColumnSize ( UINT idCol );
	void SetColumnSize ( UINT idCol, int iSize );

	int GetColumnPercent ( UINT idCol ); 
	void SetColumnPercent ( UINT idCol, int iPercent );

	int GetColumnPos( UINT idCol );
	UINT GetColumnAtPos( int iPos );
	void SetColumnPos( UINT idCol, int iColumn );

    void SetColumnName ( UINT idCol, LPCTSTR pName ); 
	LPCTSTR GetColumnName( UINT idCol ) { return NULL; }

	void SetImageColumn( UINT idCol ) { m_idImageCol = idCol; }
	void SetHasPipes( BOOL bPipes ) { m_bHasPipes = bPipes; }

	int GetNumColumns() { return m_iNumColumns; }
	void SetVisibleColumns( UINT iVisCol ) { m_iVisColumns = iVisCol; }
	UINT GetVisibleColumns() { return m_iVisColumns; }
    
	void LoadXPPrefs( const char *prefname );
	void SaveXPPrefs( const char *prefname );

	// Item stuff
	virtual void SelectItem ( int iSel, int mode = OUTLINER_SET, UINT flags = 0 );
	virtual BOOL DeleteItem ( int iLine ) { return TRUE; }
	void ScrollIntoView( int iVisibleLine );
	void EnsureVisible(int iVisibleLine);
	virtual void SetFocusLine(int iLine);
	virtual int GetFocusLine();
	virtual void SetTotalLines( int );
	virtual int GetTotalLines();

protected:

	//
	// rhp - Adding this stuff in to try to give QA partner a way to 
	// get the information out of the outliner objects. This will be
	// a single entry point used with the WM_COPYDATA message.
	//
	afx_msg LONG OnProcessOLQAHook(UINT, LONG);
	
	afx_msg int OnCreate ( LPCREATESTRUCT );
	afx_msg void OnPaint ( );
	afx_msg void OnSize ( UINT, int, int );
	afx_msg void OnGetMinMaxInfo ( MINMAXINFO FAR* lpMMI );
	afx_msg void OnDestroy ( );
	afx_msg void OnLButtonDown ( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp ( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDown ( UINT nFlags, CPoint point );
    afx_msg void OnRButtonUp( UINT nFlags, CPoint pt );
	afx_msg void OnKeyUp ( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnSysKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnLButtonDblClk ( UINT nFlags, CPoint point );
	afx_msg void OnKillFocus ( CWnd * pNewWnd );
	afx_msg void OnSetFocus ( CWnd * pOldWnd );
	afx_msg void OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	afx_msg BOOL OnEraseBkgnd( CDC * );
	afx_msg void OnTimer( UINT );
	afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	afx_msg void OnSysColorChange( );
	afx_msg UINT OnGetDlgCode( );
#if defined(XP_WIN32) && _MSC_VER >= 1100
    afx_msg LONG OnHackedMouseWheel(WPARAM wParam, LPARAM lParam);
    afx_msg LONG OnMouseWheel(WPARAM wParam, LPARAM lParam);
#endif
	DECLARE_MESSAGE_MAP()
};


class CMSelectOutliner: public COutliner 
{
protected:

	MSG_ViewIndex *m_pIndices;
	int m_iIndicesSize, m_iIndicesCount;
	int m_iShiftAnchor;
	BOOL m_bNoMultiSel;

public:
	CMSelectOutliner();
	~CMSelectOutliner();

	void GetSelection( const MSG_ViewIndex *&indices, int &count );
	virtual void SetTotalLines( int iLines );
    virtual BOOL IsSelected ( int iLine );

    virtual void PositionHome( );
    virtual void PositionEnd( );
    virtual void PositionPrevious( );
    virtual void PositionNext( );
    virtual void PositionPageUp( );
    virtual void PositionPageDown( );

	virtual void SelectItem ( int iSel, int mode = OUTLINER_SET, UINT flags = 0 );
	virtual void SelectRange( int iStart, int iEnd, BOOL bNotify );

protected:
	void AddSelection( MSG_ViewIndex iSel );
	void SelectRange( MSG_ViewIndex iSelBegin, MSG_ViewIndex iSelEnd );
	void RemoveSelection( MSG_ViewIndex iSel );
	void RemoveSelectionRange( MSG_ViewIndex iSelBegin, MSG_ViewIndex iSelEnd );
	void ClearSelection();

	virtual BOOL HandleInsert( MSG_ViewIndex iStart, LONG iCount );
	virtual BOOL HandleDelete( MSG_ViewIndex iStart, LONG iCount );
	virtual void HandleScramble();
};

class COutlinerParent: public CWnd 
{
protected:
	CNSToolTip2 m_wndTip;

    BOOL    m_bDisableHeaders;
    BOOL	m_bHasBorder;

    HFONT   m_hToolFont;

    int     m_iHeaderHeight;
    int		m_cxChar;

	POINT	m_pt, m_ptHit;
	RECT	m_rcTest;
	RECT	m_rcHit, m_rcDrag;

	HBITMAP m_hbmDrag;
	HDC		m_hdcDrag;

	BOOL	m_bEnableFocusFrame;
	BOOL	m_bResizeArea;
    BOOL    m_bResizeColumn;
	BOOL	m_bHeaderSelected;
	BOOL	m_bDraggingHeader;
    UINT    m_idColHit;
	int		m_iColHit;
	int		m_iColResize;
	int		m_iColLoser;

    LPUNKNOWN m_pUnkImage;
    LPIMAGEMAP m_pIImage;
    LPUNKNOWN m_pUnkUserImage;
    LPIMAGEMAP m_pIUserImage;

	enum { pusherNone = 0, pusherLeft = 1, pusherRight = 2, pusherLeftRight = 3 };
	int		m_iPusherWidth;
	int		m_iPusherState;
	int		m_iPusherRgn;
	int		m_iPusherHit;

	void InvalidatePusher();
	int TestPusher( POINT &pt );
	BOOL TestCol( POINT &pt, int &iCol );
	void GetColumnRect( int iCol, RECT &rc );

    virtual BOOL ColumnCommand ( int idColumn );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, LPCTSTR lpsz = NULL );

	void DrawButtonRect( HDC hDC, const RECT &rect, BOOL bDepressed );
	void DrawColumnHeader( HDC hDC, const RECT &rect, int iCol );

    BOOL ResizeClipCursor();
    
public:
    COutliner * m_pOutliner;

    COutlinerParent();
    ~COutlinerParent();

    void EnableBorder ( BOOL bEnable = TRUE ) { m_bHasBorder = bEnable; }
    void EnableHeaders ( BOOL bEnable = TRUE) { m_bDisableHeaders = !bEnable; Invalidate ( ); }
   	void EnableFocusFrame ( BOOL bEnable = TRUE) { m_bEnableFocusFrame = bEnable; }
	void SetOutliner ( COutliner * pOutliner );

	void InvalidateColumn( int iCol );
	void UpdateFocusFrame();
    
    virtual COutliner * GetOutliner ( void ) { return NULL; }
    virtual void CreateColumns ( void ) { }

protected:
	virtual BOOL PreTranslateMessage( MSG* pMsg );

	//{{AFX_MSG(COutlinerParent)
    afx_msg int OnCreate ( LPCREATESTRUCT );
    afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI );
    afx_msg void OnSetFocus ( CWnd * pOldWnd );
    afx_msg BOOL OnEraseBkgnd( CDC * );
    afx_msg void OnPaint ( void );
    afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
    afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
    afx_msg void OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags );
    afx_msg void OnMouseMove( UINT nFlags, CPoint point );
    afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()

};


///////////////////////////////////////////////////////////////////////
// COutlinerView

class COutlinerView: public CView
{
public:
    COutlinerParent * m_pOutlinerParent;

    void CreateColumns ( )
    {
        m_pOutlinerParent->CreateColumns ( );
    }

protected:
	~COutlinerView() {
		delete m_pOutlinerParent;
	}
    virtual void OnDraw(CDC *pDC);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    afx_msg int OnCreate ( LPCREATESTRUCT );
    afx_msg void OnSize ( UINT, int, int );
    afx_msg void OnSetFocus ( CWnd * pOldWnd );
    DECLARE_MESSAGE_MAP()

    DECLARE_DYNCREATE(COutlinerView)
};

//////////////////////////////////////////////////////////////////////////////
// COutlinerDropTarget declaration

class COutlinerDropTarget: public COleDropTarget
{
protected:
	DWORD m_dwOldTicks;
	COutliner *m_pOutliner;

public:
    DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
    BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
    DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
    void OnDragLeave(CWnd* pWnd);
#ifdef _WIN32
    DROPEFFECT OnDragScroll(CWnd* pWnd, DWORD dwKeyState, CPoint point);
#else
    BOOL OnDragScroll(CWnd* pWnd, DWORD dwKeyState, CPoint point);
#endif
	void DragScroll(BOOL);
	COutlinerDropTarget(COutliner *);
};


#ifndef _WIN32
HGDIOBJ GetCurrentObject(HDC hdc, UINT uObjectType);
#endif

#endif
