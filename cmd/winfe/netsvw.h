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

// netsvw.h : interface of the CNetscapeView class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef MOVIEW_H
#define MOVIEW_H

// if user moves mouse more than this distance between button down and
//  button up do not follow a link if it's over one
#define CLICK_THRESHOLD 2 

#define NETSCAPE_IMAGE_FORMAT "Netscape Image Format"
#define NETSCAPE_EDIT_FORMAT  "NETSCAPE_EDITOR"

// Number of formats registered for m_pClipboardFormats array

#ifdef _IMAGE_CONVERT
#define MAX_CLIPBOARD_FORMATS   12
#else
#define MAX_CLIPBOARD_FORMATS   11
#endif //_IMAGE_CONVERT

// Types of items being dragged over us
enum FEDragType {
    FE_DRAG_UNKNOWN,
    FE_DRAG_TEXT,
    FE_DRAG_HTML,
    FE_DRAG_TABLE,
    FE_DRAG_LINK,
    FE_DRAG_IMAGE,
	FE_DRAG_VCARD,
    FE_DRAG_FORM_ELEMENT
    // Add more as we dream them up!
};

#ifdef MOZ_NGLAYOUT
#include "nsIWebWidget.h"
#endif

/////////////////////////////////////////////////////////////////////////////
// Mostly default behavior, but override the type of cursor used 
//   during drag and drop
//
class CViewDropSource : public COleDropSource
{
public:
//Construction
    CViewDropSource(UINT nDragType = 0);
    UINT  m_nDragType;
    SCODE GiveFeedback(DROPEFFECT dropEffect);
};

/////////////////////////////////////////////////////////////////////////////
// 
class CViewDropTarget : public COleDropTarget
{
public:
//Construction
    CViewDropTarget();
    BOOL        OnDrop(CWnd *, COleDataObject *, DROPEFFECT, CPoint);
    DROPEFFECT  OnDragEnter(CWnd *, COleDataObject *, DWORD, CPoint);
    DROPEFFECT  OnDragOver(CWnd *, COleDataObject *, DWORD, CPoint);
    void        OnDragLeave(CWnd *);
private:
    int m_nIsOKDrop;
};

#ifdef EDITOR
// Implemented in edview.cpp
// Call when we MUST save a new document,
//      such as before Printing or calling an external Editor
// Return TRUE if: 
//      1. User saved the document
//      2. User didn't want to save, and doc already exists (not new)
//      3. Doc exists but is not dirty
//      4. We are not a Page Composer (allows calling without testing for EDT_IS_EDITOR() first )
//      5. We are a message Message Compose window
// Note: We must always save a new/untitled document, even if not dirty
BOOL FE_SaveDocument(MWContext * pMWContext);

// Call this instead of forcing file saving by making 
//  a temporary file from current page. This filename
//  replaces the pUrl->address in struct passed in.
//  If return is FALSE, then there was an error - don't continue
BOOL FE_PrepareTempFileUrl(MWContext * pMWContext, URL_Struct *pUrl);
#endif // EDITOR

////////////////////////////////////////////////////////////////////////////////
// Main Document view
// All stuff specific to the main document viewing area (i.e. NOT ledge stuff)
//
class CPrintCX;

class CNetscapeView : public CGenericView
{
//	Properties
public:
   CWnd * m_pChild;
	int  m_nBDownX, m_nBDownY;    // pixel coords of button down event, LFB

	// Right mouse stuff
	POINT m_ptRBDown;			// coords of right button down, not to confuse with LFB
	LO_Element*	m_pRBElement;	// The layout element we're over on a RBD.
	CString m_csRBLink;			// The link that we're over on a RBD.
	CString m_csRBImage;		// The image that we're over on a RBD.
    CString m_csRBEmbed;		// The embed that we're over on a RBD.
#ifdef LAYERS
	CL_Layer *m_pLayer;			// The layer that we're over 
#endif

    CViewDropTarget * m_pDropTarget;
    int m_SubView;  // Are we the top ledge, main view or bottom ledge

#ifdef EDITOR
    // Array of clipboard formats accepted for pasting into document
    // We put here instead of Editor's view because we need to drag 
    //   FROM a Browser window
    UINT * m_pClipboardFormats;
    
    CLIPFORMAT   m_cfEditorFormat;
    CLIPFORMAT   m_cfBookmarkFormat;
    CLIPFORMAT   m_cfImageFormat;
#endif
    // Progress dialog used for Editor Publishing and 
    //   FTP/HTTP upload
    CSaveFileDlg*  m_pSaveFileDlg;	

public:
	static void CopyLinkToClipboard(HWND hWnd, CString url);


protected: // create from serialization only
    DECLARE_DYNCREATE(CNetscapeView)
    CNetscapeView();
    ~CNetscapeView();

//	Our context, a Grid.
//	Some grid friends (creation/destruction).
//	It isn't setup until the call to SetContext.
protected:
	friend MWContext *FE_MakeGridWindow(MWContext *pOldContext, int32 lX, int32 lY, int32 lWidth, int32 lHeight, char *pUrlStr, char *pWindowName);
	friend void FE_FreeGridWindow(MWContext *pContext);

public:
	HDC GetContextDC()	{
		if(GetContext())	{
			return(GetContext()->GetContextDC());
		}
		return(NULL);
	}

// Operations
protected:
	virtual void OnInitialUpdate();	//	Called first time after construct

    virtual void OnActivateFrame(UINT nState, CFrameWnd* /*pFrameWnd*/);
    // Focus management (for internationalization)
    virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);

// Implementation
public:

    //	Print preview.
	CPrintCX *m_pPreviewContext;
    void OnFilePrintPreview();
	void OnEndPrintPreview(CDC *pDC, CPrintInfo *pInfo, POINT pXY,
		CPreviewView *pView);
	BOOL DoPrintPreview(UINT nIDResource, CView *pPrintView,
		CRuntimeClass *pPreviewViewClass, CPrintPreviewState *pState);
	void OnPrint(CDC *pDC, CPrintInfo *pInfo);
	BOOL OnPreparePrinting(CPrintInfo *pInfo);

    virtual BOOL PreCreateWindow(CREATESTRUCT & cs);
    
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

    void SetChild ( CWnd * pChild ) { m_pChild = pChild; }

#ifdef EDITOR
	BOOL   m_bIsEditor;
    inline BOOL IsEditor() { return m_bIsEditor; }

    // Get editor from preferences app via pPrefName
    // If no default, get from Open File Dialog (use nIDCaption for dialog caption)
    void CNetscapeView::SpawnExternalEditor(MWContext *pMWContext, const char *pPrefName, char *pURL, UINT nIDCaption);

    // Load an image in a user-designated editor (saved in preferences)
    void EditImage(char *pImage);  
#endif

#ifdef LAYERS
        void GetLogicalPoint(CPoint cpPoint, long *pLX, long *pLY);
	LO_Element *GetLayoutElement(CPoint cpPoint, CL_Layer *layer);
	virtual BOOL OnRButtonDownForLayer(UINT nFlags, CPoint& point, 
					   long lX, long lY, CL_Layer *layer);
#else
	LO_Element *GetLayoutElement(CPoint cpPoint);
#endif /* LAYERS */

	CString GetAnchorHref(LO_Element *pElement);
	CString GetImageHref(LO_Element *pElement);
    CString GetEmbedHref(LO_Element *pElement);

    void SetSubView(int subView)	{
    	m_SubView = subView;
    }

    CGenericDoc *GetDocument();

#ifdef EDITOR
    virtual void OnLocalPopup();
#endif /* EDITOR */
    
protected:
    // populate the given menu with items appropritate for being 
    //   over the given LO_Element
    void CreateWebPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer);
    void CreateMessagePopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer);
    BOOL AddLinkToPopup(CMenu * pMenu, LO_Element * pElement, BOOL bBrowser);
    BOOL AddEmbedToPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer, BOOL bAddSeparator);
    BOOL AddClipboardToPopup(CMenu * pMenu, LO_Element * pElement, BOOL bAddSeparator);
	BOOL AddSaveItemsToPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer, BOOL bAddSeparator);
	void AddBrowserItemsToPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer);
	void AddNavigateItemsToPopup(CMenu *pMenu, LO_Element * pElement, CL_Layer *layer);
	void AddViewItemsToPopup(CMenu * pMenu, MWContext *pContex, LO_Element * pElement);
// Used by OnPopupAddLinkToBookmarks and OnPopupInternetShortcut

	void CreateTextAndAnchor(CString &, CString &);

// Generated message map functions
protected:
    //{{AFX_MSG(CNetscapeView)
   afx_msg void OnSize ( UINT nType, int cx, int cy );
#ifndef MOZ_NGLAYOUT
    afx_msg void OnSetFocus(CWnd *);
    afx_msg void OnPrepareDC(CDC *pDC, CPrintInfo * pInfo /* = NULL */);
	afx_msg void OnDeactivateEmbed();
    afx_msg void OnWindowPosChanged(WINDOWPOS * wndpos);
    afx_msg void OnUpdateNavigateRepaint(CCmdUI* pCmdUI);
	afx_msg void OnMove(int x, int y);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnCopyImageToClipboard();
	afx_msg void OnPopupLoadLink();
	afx_msg void OnPopupAddLinkToBookmarks();
	afx_msg void OnPopupLoadLinkNewWindow();
	afx_msg void OnPopupLoadFrameNewWindow();
	afx_msg void OnPopupSaveLinkContentsAs();
	afx_msg void OnPopupCopyLinkToClipboard();
	afx_msg void OnPopupLoadImage();
	afx_msg void OnPopupSaveImageAs();
	afx_msg void OnPopupCopyImageLocationToClipboard();
	afx_msg void OnPopupLoadImageInline();
	afx_msg void OnPopupActivateEmbed();
	afx_msg void OnPopupSaveEmbedAs();
	afx_msg void OnCopyCurrentURL();
	afx_msg void OnPopupCopyEmbedLocationToClipboard();
    afx_msg void OnPopupCopyEmbedToClipboard();
	afx_msg void OnPopupSetAsWallpaper();
    afx_msg void OnNavigateRepaint();
	afx_msg	BOOL OnQueryNewPalette(); 
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPopupInternetShortcut();
	afx_msg void OnPopupMailTo();

    // afx_msg void OnDropFiles( HDROP hDropInfo );
#endif /* MOZ_NGLAYOUT */
	//}}AFX_MSG
#ifdef EDITOR
    afx_msg void OnCanInteract(CCmdUI* pCmdUI);
    afx_msg void OnEditSource();
    afx_msg void OnPopupEditImage();
    afx_msg void OnPopupLoadLinkInEditor();
#endif
    DECLARE_MESSAGE_MAP()

#ifdef MOZ_NGLAYOUT
private:
  void checkCreateWebWidget();
  BOOL m_bNoWebWidgetHack;

public:
  void NoWebWidgetHack() {m_bNoWebWidgetHack = TRUE;}
  // Hack to disable it for the RDF window.
#endif /* MOZ_NGLAYOUT */
};

#ifdef _DEBUG_HUH_JEM  // debug version in netsvw.cpp
inline CNetscapeDoc* CNetscapeView::GetDocument()
   { return (CNetscapeDoc*) m_pDocument; }
#endif
/////////////////////////////////////////////////////////////////////////////

#endif //MOVIEW_H
