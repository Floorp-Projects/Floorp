/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __WindowContext_H
//	Avoid include redundancy
//
#define __WindowContext_H

//	Purpose:	Provide an abstract context in a window visible on a screen.
//	Comments:	Ill named, as the window is actually a view.
//				Must be derived from to become useful.
//	Revision History:
//		06-25-95	created GAB
//

//	Required Includes
//
#include "cxpane.h"
#include "frameglu.h"
#include "xp_core.h"
#include "xp_list.h"

#include "tooltip.h"

class CWinCX : public CPaneCX	{
//  The view makes a comeback for performance reasons.
//  Leave at the top, forces crashers for bad casts in some cases.
private:
    CGenericView *m_pGenView;
public:
    CGenericView *GetView() { return m_pGenView; }
	
//	Construct, Destruction, Indirect construction
public:
	CWinCX(CGenericDoc *pDoc, CFrameGlue *pFrame, CGenericView *pView, MWContextType mwType = MWContextBrowser, ContextType cxType = Window);
	~CWinCX();
	virtual void DestroyContext();
	virtual void Initialize(BOOL bOwnDC, RECT *pRect = NULL, BOOL bInitialPalette = TRUE, BOOL bNewMemDC = TRUE);

//	Null frame handling (absence of a frame handling)
public:
	static CNullFrame *m_pNullFrame;
	static void *m_pExitCookie;

//	The view/frame working with this context.
//	We own a document (see CDCCX).
//	The OLE friend mucks with this.
	friend class CInPlaceFrame;
protected:
#ifdef DDRAW
	LPDIRECTDRAW  m_lpDD;           // DirectDraw object
	LPDIRECTDRAWSURFACE     m_lpDDSPrimary;   // DirectDraw primary surface
	LPDIRECTDRAWSURFACE     m_lpDDSBack;      // DirectDraw back surface
	LPDIRECTDRAWCLIPPER		m_pClipper;
	LPDIRECTDRAWCLIPPER		m_pOffScreenClipper;
	LPDIRECTDRAWPALETTE     mg_pPal;
	DDSURFACEDESC			m_surfDesc;
	BOOL m_ScrollWindow;
	LTRB m_physicWinRect;
	HDC m_offScreenDC;
#endif
	CFrameGlue *m_pFrame;
	XP_List* imageToolTip;
	CNSToolTip *m_ToolTip;
	LO_ImageStruct* pLastToolTipImg;
	LO_AnchorData*	m_pLastToolTipAnchor;

public:
	CFrameGlue *GetFrame() const;
	void ClearView();
	void ClearFrame();

	CNSToolTip*	CreateToolTip(LO_ImageStruct* pImage, CPoint& point, CL_Layer *layer);
	void RelayToolTipEvent(POINT pt, UINT message);
        void ClipChildren(CWnd *pWnd, BOOL bSet);
//	Palette Access
public:
	virtual HPALETTE GetPalette() const;
#ifdef DDRAW
	virtual int	 DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, LTRB& Rect);
#endif
//	CDC Access
public:
	virtual HDC GetDispDC() 
	{
#ifdef DDRAW
		if (m_offScreenDC) return m_offScreenDC;
		else return GetContextDC();
#else
		return GetContextDC();
#endif
	}
#ifdef DDRAW
	void CalcWinPos();
	int GetWindowsXPos() {return (int)m_physicWinRect.left;}
	int GetWindowsYPos() {return (int)m_physicWinRect.top;}
	virtual void LockOffscreenSurfDC() {	m_lpDDSBack->GetDC(&m_offScreenDC); }
	virtual void ReleaseOffscreenSurfDC()
		{	if (m_lpDDSBack) m_lpDDSBack->ReleaseDC(m_offScreenDC);}
	void ReleaseDrawSurface(); 
	void RestoreAllDrawSurface();
	void ScrollWindow(int x, int y);
	LPDIRECTDRAWCLIPPER GetClip() {return m_pClipper;}
	LPDIRECTDRAWSURFACE GetPrimarySurface() {return m_lpDDSPrimary;}
	LPDIRECTDRAWSURFACE GetBackSurface() {return m_lpDDSBack;}
	LPDIRECTDRAW GetDrawObj() {return m_lpDD;}
	LPDDSURFACEDESC	GetSurfDesc() {return &m_surfDesc;}
	void SetClipOnDrawSurface(LPDIRECTDRAWSURFACE surface, HRGN hClipRgn);
	LPDIRECTDRAWSURFACE CreateOffscreenSurface(RECT& rect);
	void CreateAndLockOffscreenSurface(RECT& rect);
	void BltToScreen(LTRB& rect, DDBLTFX* fx = NULL);
#endif

	void ResetToolTipImg();

//	For dialogs
public:
	virtual CWnd *GetDialogOwner() const;

//	When in layout, we need a special case to detect if we need to reload the document
//		if the user resizes the window.
protected:
	BOOL m_bIsLayingOut;	//	Set in LayoutNewDocument and LayoutFinished

public:
	BOOL IsLayingOut() const	{
		return(m_bIsLayingOut);
	}
//	Can we accept mouse clicks?
	BOOL IsClickingEnabled() const;

//	Cached mouse hits.
public:
	CPoint m_cpLBDClick;
	CPoint m_cpLBDown;
	CPoint m_cpLBUp;
	CPoint m_cpMMove;
	CPoint m_cpRBDClick;
	CPoint m_cpRBDown;
	CPoint m_cpRBUp;
//	Mouse state.
public:
    enum MouseEvent {
        m_None,
        m_LBDClick,
        m_LBDown,
        m_LBUp,
        m_MMove,
        m_RBDClick,
        m_RBDown,
        m_RBUp
    } m_LastMouseEvent;
	UINT m_uMouseFlags;
	BOOL m_bLBDown;
	BOOL m_bLBUp;

//	Timer processing.
public:
    BOOL m_bScrollingTimerSet;
    BOOL m_bMouseMoveTimerSet;

//  Currently selected embedded item, or NULL on none.
public:
    LO_EmbedStruct *m_pSelected;

//	Last highlighted anchor, NULL if none.
protected:
	LO_Element *m_pLastArmedAnchor;

//	Window notifications.
//	These are pretty much identical to MFC functions names with the added CX
protected:
    virtual void AftWMSize(PaneMessage *pMessage);
public:
#ifdef EDITOR
    virtual void Scroll(int iBars, UINT uSBCode, UINT uPos, HWND hCtrl, UINT uTimes = 1);
#endif
	virtual void OnMoveCX();
	virtual void OnDeactivateEmbedCX();

#ifdef LAYERS
    virtual BOOL HandleLayerEvent(CL_Layer * pLayer, CL_Event * pEvent);
    virtual BOOL HandleEmbedEvent(LO_EmbedStruct *embed, CL_Event * pEvent);
#endif /* LAYERS */
    virtual void OnLButtonDblClkCX(UINT uFlags, CPoint cpPoint);
    virtual void OnLButtonDownCX(UINT uFlags, CPoint cpPoint);
    virtual void OnLButtonUpCX(UINT uFlags, CPoint cpPoint, BOOL &bReturnImmediately);
    virtual void OnMouseMoveCX(UINT uFlags, CPoint cpPoint, BOOL &bReturnImmediately);
    virtual void OnRButtonDblClkCX(UINT uFlags, CPoint cpPoint);
    virtual void OnRButtonDownCX(UINT uFlags, CPoint cpPoint);
    virtual void OnRButtonUpCX(UINT uFlags, CPoint cpPoint);
    virtual BOOL OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    virtual BOOL OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);

#ifdef LAYERS
	// These are the per-layer event handlers. 
	virtual void OnLButtonDblClkForLayerCX(UINT uFlags, CPoint& cpPoint,
					       XY& xyPoint, CL_Layer *layer);
	virtual void OnLButtonDownForLayerCX(UINT uFlags, CPoint& cpPoint,
					     XY& xyPoint, CL_Layer *layer);
	virtual void OnLButtonUpForLayerCX(UINT uFlags, CPoint& cpPoint,
					   XY& xyPoint, CL_Layer *layer, BOOL &bReturnImmediately);
	virtual void OnMouseMoveForLayerCX(UINT uFlags, CPoint& cpPoint,
					   XY& xyPoint, CL_Layer *layer, BOOL &bReturnImmediately);
	virtual BOOL OnRButtonDownForLayerCX(UINT uFlags, CPoint& cpPoint,
					     XY& xyPoint, CL_Layer *layer);
	virtual void OnRButtonDblClkForLayerCX(UINT uFlags, CPoint& cpPoint,
					       XY& xyPoint, CL_Layer *layer);
	virtual void OnRButtonUpForLayerCX(UINT uFlags, CPoint& cpPoint,
					   XY& xyPoint, CL_Layer *layer);
#endif /* LAYERS */

	virtual void OnNcPaintCX();
    virtual void OnNcCalcSizeCX(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);

//	This is used to track the window rect between OnSize
//		messages.  If the actual window rect has not
//		changed, then expect that we do not need to
//		redraw the view (InValidate/UpdateWindow).
private:
	CRect m_crWindowRect;

public:
    // Analogous to m_pLastArmedAnchor,
    //  remember an image we click on
    //  to do drag n drop to editor
    // Public so the view can access it for
    //  copy-image-to-clipboard
	LO_ImageStruct *m_pLastImageObject;

	int32 RightMargin()	{
		return(m_lRightMargin);
	}
            
//  URL Retrieval.
public:
    virtual int GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoad = TRUE, BOOL bForceNew = FALSE);

//	Progress helpers.
protected:
	int32 m_lOldPercent;
public:
	int32 QueryProgressPercent();
	void StartAnimation();
	void StopAnimation();

// #ifdef EDITOR
    // Test if screen point, such mouse cursor, 
    //      is within a single selected region.
    BOOL PtInSelectedRegion(CPoint cPoint, BOOL bConvertToDocCoordinates = FALSE, CL_Layer *layer = NULL);
    BOOL PtInSelectedCell(CPoint &DocPoint, LO_CellStruct *cell, 
                          BOOL &bContinue, LO_Element *start_element,
                          LO_Element *end_element);

    // Flags used during drag/drop
    BOOL m_bMouseInSelection;
    // Drag current doc URL from the URL Bitmap to whatever
    //   destination accepts a Bookmark OLE struct or URL text
    void DragCurrentURL();
    void CopyCurrentURL();
    void DragSelection();

    // TRUE when we are dragging something from this context
    BOOL m_bDragging;
    
    // Browser doesn't need this, but Editor
    //  does to prevent errant mouse-over message
    //  which results in wrong cursor for right-button popup menu
    BOOL m_bInPopupMenu;

    LTRB m_rectSizing;

    // Used only in Editor when dragging to select multiple cells
    BOOL m_bSelectingCells;

public:
    void CancelSizing();
    BOOL IsDragging()  { return(m_bDragging); }

//	Targeted load operations.
public:
	CWinCX *DetermineTarget(const char *pTarget);

//	Saving what is currently in the frames location bar,
//		so we don't go blowing it away.
protected:
	CString m_csSaveLocationBarText;

//	Funtion to get our view's offset into the frame window.
public:
	void GetViewOffsetInFrame(CPoint& cpRetval)	const;

//	Function to tell the context it is now the active
//		context in a frame.
private:
	BOOL m_bActiveState;
public:
	BOOL IsActive() const	{
		return(m_bActiveState);
	}
	void ActivateCX(BOOL bNowActive);

//	Modality support.
public:
	void GoModal(MWContext *pModalOver);
protected:
	CPtrList m_cplModalOver;

//	Close callback support.
private:
    CPtrList m_cplCloseCallbacks;
    CPtrList m_cplCloseCallbackArgs;
public:
	void CloseCallback(void (*)(void *), void *);

//	Resize and close disabling.
public:
	void EnableResize(BOOL bEnable = TRUE);
	void EnableClose(BOOL bEnable = TRUE);

//  Wether or not our size was determined via chrome.
public:
    BOOL m_bSizeIsChrome;

//Z order locking
	void SetZOrder(BOOL bZLock = FALSE, BOOL bBottommost = FALSE);
	void DisableHotkeys(BOOL bDisable = FALSE);

//  Wether or not we attempt to draw a border.
private:
    BOOL m_bHasBorder;
public:
    void SetBorder(BOOL bBorder)    {
        m_bHasBorder = bBorder;
    }

//  How to correclty handle mouse entering element
//      regions and reporting this to the backend.
// Note: The element pointers are not valid after relayout - 
//   must call ClearLastElemen() just before or after a relayout
protected:
    BOOL m_bLastOverTextSet;
    LO_Element *m_pLastOverElement;
    LO_AnchorData *m_pLastOverAnchorData;
    LO_Element *m_pStartSelectionCell;

//#ifndef NO_TAB_NAVIGATION 
public :
    void        ClearLastElement() {m_pLastOverElement = NULL; m_pLastOverAnchorData = NULL; m_pStartSelectionCell = NULL;}
	int32			getLastFocusAreaIndex();
	LO_AnchorData	*CWinCX::getLastFocusAnchorPointer();
	char			*CWinCX::getLastFocusAnchorStr();

	LO_TabFocusData * CWinCX::getLastTabFocusData();
	LO_Element	*CWinCX::getLastTabFocusElement();
	int			CWinCX::invalidateSegmentedTextElement( LO_TabFocusData *pNextTabFocus, int forward );
	void        CWinCX::setFormElementTabFocus( LO_Element * pFormElement );

	void		CWinCX::invalidateElement( LO_Element *pElement );
	BOOL		CWinCX::setTabFocusNext( int forward );	// try myself and then siblings
	BOOL		CWinCX::setNextTabFocusInWin( int forward );	// only in myself
	BOOL		CWinCX::setTabFocusNextChild( MWContext *currentChildContext, int forward ); // as a parent, try other children

	BOOL		CWinCX::fireTabFocusElement( UINT nChar);	// WPARAM vKey );
	int			CWinCX::setTextTabFocusDrawFlag( LO_TextStruct *pText, uint32 *pFlag );
	int			CWinCX::getImageDrawFlag(MWContext *context, LO_ImageStruct *pImage, lo_MapAreaRec **ppArea, uint32 *pFlag );
	void		CWinCX::DisplayFeedback(MWContext *pContext, int iLocation, LO_Element *pElement);

private :
	void		CWinCX::SetActiveWindow();
    void        CWinCX::setLastTabFocusElement( LO_TabFocusData *pNextTabFocus, int needSetFocus ); 
	void		CWinCX::SetMainFrmTabFocusFlag( int nn );
	void		CWinCX::ClearMainFrmTabFocusFlag( void );

private :
	LO_TabFocusData	m_lastTabFocus;	
    int         m_isReEntry_setLastTabFocusElement;     // to provent re-entry
	BOOL		SaveOleDocument();

//#endif /* NO_TAB_NAVIGATION */



public:
    LO_AnchorData *GetAreaAnchorData(LO_Element *pElement);
    void FireMouseOutEvent(BOOL bClearElement, BOOL bClearAnchor, int32 xVal, int32 yVal,
                           CL_Layer *layer);
    void FireMouseOverEvent(LO_Element *pElement, int32 xVal, int32 yVal,
                            CL_Layer *layer);

//	Misc MFC helpers.
public:
	virtual void OpenFile();
	virtual BOOL CanOpenFile();
	virtual void SaveAs();
	virtual BOOL CanSaveAs();
	virtual void PrintContext();
	virtual void Print();
	virtual BOOL CanPrint(BOOL bPreview = FALSE);
	virtual void AllFind(MWContext *pSearchContext = NULL);
	virtual BOOL CanAllFind();
	virtual void FindAgain();
	virtual BOOL CanFindAgain() const { return(!theApp.m_csFindString.IsEmpty()); }
	virtual BOOL DoFind(CWnd * pWnd, const char * pFindWhat, BOOL bMatchcase, 
					    BOOL bSearchDown, BOOL bAlertOnNotFound);
	virtual BOOL CanViewSource();
	virtual void ViewSource();
	virtual BOOL CanDocumentInfo();
	virtual void DocumentInfo();
	virtual BOOL CanFrameSource();
	virtual void FrameSource();
	virtual BOOL CanFrameInfo();
	virtual void FrameInfo();
	virtual BOOL CanGoHome();
	virtual void GoHome();
	virtual BOOL CanUploadFile();
	virtual void UploadFile();

//	Context Overrides
public:
    virtual void LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight);
	virtual void FinishedLayout(MWContext *pContext);
	virtual void AllConnectionsComplete(MWContext *pContext);
	virtual void UpdateStopState(MWContext *pContext);
	virtual void SetDocTitle(MWContext *pContext, char *pTitle);
	virtual void SetInternetKeyword(const char *keyword);
	virtual void ClearView(MWContext *pContext, int iView);
    virtual void CreateEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp);
    virtual void SaveEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp);
    virtual void RestoreEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp);
    virtual void DestroyEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp);
	virtual void DisplayJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *java_struct);
#ifdef TRANSPARENT_APPLET
	virtual void HandleClippingView(MWContext *pContext, LJAppletData *appletD, int x, int y, int width, int height);
#endif
	virtual void HideJavaAppElement(MWContext *pContext, LJAppletData * session_data);
	virtual void SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength);
	virtual void SetDocPosition(MWContext *pContext, int iLocation, int32 lX, int32 lY);
	virtual void FreeJavaAppElement(MWContext *pContext, LJAppletData *appletD);
	virtual void GetJavaAppSize(MWContext *pContext, LO_JavaAppStruct *java_struct, NET_ReloadMethod bReload);
	virtual void DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool clear);
	virtual void DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool clear);
	virtual void SetProgressBarPercent(MWContext *pContext, int32 lPercent);
	virtual void Progress(MWContext *pContext, const char *pMessage);
#ifndef MOZ_NGLAYOUT
	virtual void DisplayEdge(MWContext *pContext, int iLocation, LO_EdgeStruct *pEdge);
	virtual void FreeEdgeElement(MWContext *pContext, LO_EdgeStruct *pEdge);
#endif
	virtual void EnableClicking(MWContext *pContext);

//	Misc draw helpers
public:
#ifdef EDITOR
    virtual void PreWMErasebkgnd(PaneMessage *pMsg);
#endif

	// Helper for erasing text structures
	BOOL	EraseTextBkgnd(HDC pDC, RECT&, LO_TextStruct*);
};

//	Global variables
//

//	Macros
//

//	Function declarations
//

#endif // __WindowContext_H
