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
#ifndef __HTML_PANE_CONTEXT_H
#define __HTML_PANE_CONTEXT_H

//  What is CPaneCX?
//      A pane is part of a complete window.
//
//      There is no relation from a pane to the chrome of a full window.
//      There is no frame, no document, no view, no toolbar, etc.
//      No assumptions should be made, or the pane becomes non-reusable in a
//          window that is yet to be invented.
//
//      A pane can be put many places:
//          In a dialog.
//          In a navigation tool.
//          In a browser.
//      It is up to the place to provide the chrome.  It is up to the pane to
//          provide the HTML.

#include "cxdc.h"
#include "drawable.h"

#define VSCROLL_LINE    32
#define HSCROLL_LINE    32
#define SCROLL_UNITS    10000
#define RIGHT_MARGIN    (-1 * Pix2TwipsX(8))
#define LEFT_MARGIN     (Pix2TwipsX(8))
#define TOP_MARGIN      (Pix2TwipsY(8))
#define BOTTOM_MARGIN   (-1 * Pix2TwipsY(8))

struct PaneMessage {
    PaneMessage(WPARAM wSet, LPARAM lSet)
    {
        wParam = wSet;
        lParam = lSet;
        bSetRetval = FALSE;
        lRetval = NULL;
    }
    WPARAM wParam;
    LPARAM lParam;
    BOOL bSetRetval;
    LPARAM lRetval;
};

class CPaneCX : public CDCCX    {
public:
    CPaneCX(HWND hPane, BOOL bDestroyOnWMDestroy);
    ~CPaneCX();
    virtual void DestroyContext();
    virtual void Initialize(BOOL bOwnDC, RECT *pRect = NULL, BOOL bInitialPalette = TRUE, BOOL bNewMemDC = TRUE);

private:
    HWND m_hPane;
    BOOL m_bDestroyOnWMDestroy;
private:
    BOOL SubClass(HWND hWnd, BOOL bSubClass);
    void CacheDCInfo(BOOL bCache);
public:
    HWND GetPane() const;
    HWND SetPane(HWND hPane);

public:
    BOOL IsNavCenterHTMLPane() const;

private:
    WNDPROC m_pPrevProc;
private:
    WNDPROC GetSubclassedProc();
    friend LRESULT CALLBACK
#ifndef _WIN32
        _export
#endif
        PaneProc(HWND hPane, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    void PreWMPaint(PaneMessage *pMsg);
#ifdef EDITOR
    virtual
#endif
    void PreWMErasebkgnd(PaneMessage *pMsg);
    void PreWMVScroll(PaneMessage *pMsg);
    void PreWMHScroll(PaneMessage *pMsg);
    void PreNavCenterQueryPosition(PaneMessage *pMsg);
    void PreIdleUpdateCmdUI(PaneMessage *pMsg);
#if defined(XP_WIN32) && _MSC_VER >= 1100
    void PreWMMouseWheel(PaneMessage *pMsg);
    void PreWMHackedMouseWheel(PaneMessage *pMsg);
#endif
protected:
    virtual void AftWMSize(PaneMessage *pMsg);
    void AftWMDestroy(PaneMessage *pMsg);
    void AftWMMouseActivate(PaneMessage *pMsg);

private:
    HDC m_hClassDC;
    HDC m_hOwnDC;
    HDC m_hSubstDC;
public:
    void RefreshArea(int32 lLeft, int32 lTop, uint32 lWidth, uint32 lHeight);
    virtual HDC GetContextDC();
    virtual void ReleaseContextDC(HDC hDC);
    virtual BOOL IsOwnDC() const;
    virtual BOOL IsClassDC() const;
    HDC SubstituteDC(HDC hDC);
    
private:
    CDrawable *m_pDrawable;
    COffscreenDrawable *m_pOffscreenDrawable;
    COnscreenDrawable *m_pOnscreenDrawable;
public:
    CDrawable *GetDrawable();
    virtual void SetDrawable(MWContext *pContext, CL_Drawable *pDrawable);
    virtual void GetDrawingOrigin(int32 *plOrgX, int32 *plOrgY);
    virtual FE_Region GetDrawingClip();

public:
    void *m_pResizeReloadTimeout;
protected:
    void NiceResizeReload(void);
    
private:
    int32 m_nPageY;
    int32 m_nPageX;
    BOOL m_bVScrollBarOn;
    BOOL m_bHScrollBarOn;
    BOOL m_bDynamicScrollBars;
    BOOL m_bAlwaysShowScrollBars;
#ifdef XP_WIN32
    int m_iWheelDelta;
#endif
public:
    void ShowScrollBars(int iBars, BOOL bShow);
    BOOL IsVScrollBarOn();
    BOOL IsHScrollBarOn();
    BOOL DynamicScrollBars();
    BOOL AlwaysShowScrollBars();
    int32 GetPageY();
    int32 GetPageX();
    void SetAlwaysShowScrollBars(BOOL bSet);
    void SetDynamicScrollBars(BOOL bSet);
    void RealizeScrollBars(int32 *pX = NULL, int32 *pY = NULL);
#ifdef EDITOR
    virtual
#endif
    void Scroll(int iBars, UINT uSBCode, UINT uPos, HWND hCtrl, UINT uTimes = 1);
    void MakeElementVisible(int32 lX, int32 lY);
    
#ifdef XP_WIN16
    // 16-bit forms need their own segment.
private:
    HGLOBAL m_hTextElementSegment;
    LPVOID  m_lpTextElementHeap;
public:
    HINSTANCE GetSegment();
#endif
    
//  Marginal implementation.
protected:
    int32 m_lLeftMargin;
    int32 m_lRightMargin;
    int32 m_lTopMargin;
    int32 m_lBottomMargin;
    
//  Coordinate resolution.
public:
    void ResolvePoint(XY& xy, POINT& point);
    virtual void GetWindowOffset(int32 *pX, int32 *pY);
    LO_Element *GetLayoutElement(XY& Point, CL_Layer *layer) const;

public:
    virtual int GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoad = TRUE, BOOL bForceNew = FALSE);
public:
    virtual void LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight);
    virtual void FinishedLayout(MWContext *pContext);
    virtual void SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength);
};

inline HWND CPaneCX::GetPane() const
{
    return(m_hPane);
}
inline WNDPROC CPaneCX::GetSubclassedProc()
{
    return(m_pPrevProc);
}
inline CDrawable *CPaneCX::GetDrawable()
{
    return(m_pDrawable);
}
inline BOOL CPaneCX::IsVScrollBarOn()
{
    return(m_bVScrollBarOn);
}
inline BOOL CPaneCX::IsHScrollBarOn()
{
    return(m_bHScrollBarOn);
}
inline BOOL CPaneCX::DynamicScrollBars()
{
    return(m_bDynamicScrollBars);
}
inline BOOL CPaneCX::AlwaysShowScrollBars()
{
    return(m_bAlwaysShowScrollBars);
}
inline int32 CPaneCX::GetPageY()
{
    return(m_nPageY);
}
inline int32 CPaneCX::GetPageX()
{
    return(m_nPageX);
}
inline void CPaneCX::SetAlwaysShowScrollBars(BOOL bSet)
{
    m_bAlwaysShowScrollBars = bSet;
}
inline void CPaneCX::SetDynamicScrollBars(BOOL bSet)
{
    m_bDynamicScrollBars = bSet;
}
inline void CPaneCX::ResolvePoint(XY& xy, POINT& point)
{
    xy.x = Pix2TwipsX(point.x) + GetOriginX();
    xy.y = Pix2TwipsY(point.y) + GetOriginY();
}
inline LO_Element *CPaneCX::GetLayoutElement(XY& Point, CL_Layer *layer) const
{
    return(LO_XYToElement(GetContext(), Point.x, Point.y, layer));
}
inline BOOL CPaneCX::IsNavCenterHTMLPane() const
{
    BOOL bRetval = FALSE;
    MWContext *pContext = GetContext();
    if(pContext && MWContextPane == pContext->type && Pane == GetContextType()) {
        bRetval = TRUE;
    }
    return(bRetval);
}

#endif // __HTML_PANE_CONTEXT_H
