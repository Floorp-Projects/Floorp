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

#include "stdafx.h"
#include "cxpane.h"
#ifdef XP_WIN32
#include "intelli.h"
#endif
#include "navcontv.h"

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

#ifdef XP_WIN32
#define STR_CPANECX "CPaneCX"
#else
#define STR_CPANECX_HI "CPaneCXHi"
#define STR_CPANECX_LO "CPaneCXLo"
#endif

CPaneCX::CPaneCX(HWND hPane, BOOL bDestroyOnWMDestroy)
{
    MWContext *pContext = GetContext();

    m_MM = MM_TEXT;

    m_bDestroyOnWMDestroy = bDestroyOnWMDestroy;
    m_hPane = NULL;
    m_pPrevProc = NULL;
    m_hOwnDC = NULL;
    m_hClassDC = NULL;
    m_hSubstDC = NULL;

    m_cxType = Pane;
    pContext->type = MWContextPane;

    m_pDrawable = NULL;
    m_pOffscreenDrawable = NULL;
    m_pOnscreenDrawable = NULL;

    m_pResizeReloadTimeout = NULL;

    m_bDynamicScrollBars = TRUE;
    m_bAlwaysShowScrollBars = FALSE;
    m_bHScrollBarOn = FALSE;
    m_bVScrollBarOn = FALSE;
#ifdef _WIN32
    m_iWheelDelta = 0;
#endif

#ifdef XP_WIN16
    m_hTextElementSegment = NULL;
    m_lpTextElementHeap = NULL;
#endif

    //  If a pane was passed in, perform the step manually.
    SetPane(hPane);
}

CPaneCX::~CPaneCX()
{
    SetPane(NULL);

    // Destroy the compositor associated with the context
    MWContext *pContext = GetContext();
    if (pContext && pContext->compositor) {
        CL_DestroyCompositor(pContext->compositor);
        pContext->compositor = NULL;
    }
    
    if(m_pOnscreenDrawable) {
        delete m_pOnscreenDrawable;
        m_pOnscreenDrawable = NULL;
    }
    if(m_pOffscreenDrawable) {
        delete m_pOffscreenDrawable;
        m_pOffscreenDrawable = NULL;
    }
    m_pDrawable = NULL;

#ifdef XP_WIN16
    if(m_hTextElementSegment) {
        ::GlobalFree(m_hTextElementSegment);
        m_hTextElementSegment = NULL;
        m_lpTextElementHeap = NULL;
    }
#endif
}

//  Set current pane for context.
//  Can switch panes from one to another (wow).
//  Can pass in NULL, which detaches current pane.
HWND CPaneCX::SetPane(HWND hPane)
{
    HWND hRetval = GetPane();
    
    if(hRetval) {
        //  Need to release all cached information.
        CacheDCInfo(FALSE);
        
        //  Need to unsubclass old window.
        BOOL bSub = SubClass(hRetval, FALSE);
        ASSERT(bSub);
        
        //  Destroy any pending reload.
        if(m_pResizeReloadTimeout) {
            FE_ClearTimeout(m_pResizeReloadTimeout);
            m_pResizeReloadTimeout = NULL;
        }
    }
    
    m_hPane = hPane;
    
    if(m_hPane) {
        //  Need to subclass new window.
        BOOL bSub = SubClass(m_hPane, TRUE);
        ASSERT(bSub);
        
        //  Need to gather all cached information.
        CacheDCInfo(TRUE);

        //  Reflect current scroll bar state.
        ::EnableScrollBar(m_hPane, SB_VERT, ESB_ENABLE_BOTH);
        ::EnableScrollBar(m_hPane, SB_HORZ, ESB_ENABLE_BOTH);
        ::SetScrollRange(m_hPane, SB_VERT, 0, SCROLL_UNITS, FALSE);
        ::SetScrollRange(m_hPane, SB_HORZ, 0, SCROLL_UNITS, FALSE);
        ::ShowScrollBar(m_hPane, SB_BOTH, FALSE);
        ShowScrollBars(SB_HORZ, IsHScrollBarOn());
        ShowScrollBars(SB_VERT, IsVScrollBarOn());
    }
    
    //  Shouldn't have a substituted DC at this point.
    ASSERT(m_hSubstDC == NULL);
    
    return(hRetval);
}

void CPaneCX::CacheDCInfo(BOOL bCache)
{
    if(bCache) {
        //  Determine window class name.
        char aClassName[128];
        memset(aClassName, 0, sizeof(aClassName));
        int iLen = GetClassName(GetPane(), aClassName, sizeof(aClassName));
        ASSERT(iLen && iLen < sizeof(aClassName));
        
        //  Fill in WNDCLASS.
        //  This needs to be expanded if we ever attempt to take over
        //      windows outside of our instance.
        WNDCLASS PaneClass;
        memset(&PaneClass, 0, sizeof(PaneClass));
        BOOL bClass = GetClassInfo(AfxGetInstanceHandle(), aClassName, &PaneClass);
        ASSERT(bClass);
        
        //  OwnDC
        if(PaneClass.style & CS_OWNDC) {
            SetOwnDC(TRUE);
            m_hOwnDC = ::GetDC(GetPane());
        }
        else if(PaneClass.style & CS_CLASSDC) {
            SetClassDC(TRUE);
            m_hClassDC = ::GetDC(GetPane());
        }
        else {
            SetOwnDC(FALSE);
            SetClassDC(FALSE);
        }
    }
    else {
        //  Any persistent data needs to go.
        ClearFontCache();
        
        //  OwnDC
        SetOwnDC(FALSE);
        m_hOwnDC = NULL;
        SetClassDC(FALSE);
        m_hClassDC = NULL;
    }
}

BOOL CPaneCX::IsOwnDC() const
{
    BOOL bRetval = FALSE;
    
    if(m_pDrawable && (m_pDrawable == m_pOffscreenDrawable)) {
        bRetval = FALSE;
    }
    else if(m_hSubstDC) {
        //  We want substituted DCs to act like OwnDCs
        //      until unsubstituted.
        bRetval = TRUE;
    }
    else {
        bRetval = CDCCX::IsOwnDC();
    }
    
    return(bRetval);
}

BOOL CPaneCX::IsClassDC() const
{
    BOOL bRetval = FALSE;
    
    if(m_pDrawable && (m_pDrawable == m_pOffscreenDrawable)) {
        bRetval = FALSE;
    }
    else {
        bRetval = CDCCX::IsClassDC();
    }
    
    return(bRetval);
}

HDC CPaneCX::SubstituteDC(HDC hDC)
{
    //  Flush cached info for old DC (take away any attributes
    //      we may have assumed as OwnDC).
    //  This could be further optimized if the font code
    //      didn't hold onto the DC for the life of a font.
    if(m_hSubstDC) {
        ClearFontCache();
    }
    
    HDC hRetval = m_hSubstDC;
    m_hSubstDC = hDC;
    
    //  Set new mapping mode for new DC.
    if(m_hSubstDC) {
        SetMappingMode(m_hSubstDC);
    }
    return(hRetval);
}

//  Return appropriate HDC
HDC CPaneCX::GetContextDC()
{
    HDC hRetval = NULL;
    if(m_pDrawable && (m_pDrawable == m_pOffscreenDrawable)) {
        hRetval = m_pDrawable->GetDrawableDC();
    }
    else if(m_hSubstDC) {
        hRetval = m_hSubstDC;
    }
    else if(IsClassDC()) {
        hRetval = m_hClassDC;
    }
    else if(CDCCX::IsOwnDC()) {
        hRetval = m_hOwnDC;
    }
    else if(GetPane()) {
        hRetval = ::GetDC(GetPane());
        SetMappingMode(hRetval);
    }
    
    return(hRetval);
}

//  Release appropriate HDC
void CPaneCX::ReleaseContextDC(HDC hDC)
{
    if(hDC != m_hSubstDC && IsClassDC() == FALSE && CDCCX::IsOwnDC() == FALSE && (!m_pOffscreenDrawable || hDC != m_pOffscreenDrawable->GetDrawableDC())) {
        ASSERT(GetPane());
        ::ReleaseDC(GetPane(), hDC);
    }
}

void CPaneCX::DestroyContext()
{
    if(IsDestroyed() == FALSE) {
        if(m_pResizeReloadTimeout)  {
            FE_ClearTimeout(m_pResizeReloadTimeout);
            m_pResizeReloadTimeout = NULL;
        }
    }
    
    CDCCX::DestroyContext();
}

void CPaneCX::Initialize(BOOL bOwnDC, RECT *pRect, BOOL bInitialPalette, BOOL bNewMemDC)
{
    MWContext *pContext = GetContext();

    //  Top of the document.
    m_lOrgX = 0;
    m_lOrgY = 0;
    
    //  Call base.
    CDCCX::Initialize(bOwnDC, pRect, bInitialPalette, bNewMemDC);
    
    //  Get the DC.
    HDC hDC = GetContextDC();
    SetMappingMode(hDC);
    
    //  Init compositor.
    CL_Drawable *pOnscreenDrawable = NULL;
    CL_Drawable *pOffscreenDrawable = NULL;
    
    m_pOnscreenDrawable = new COnscreenDrawable(hDC, this);
    pOnscreenDrawable = CL_NewDrawable(
        CASTUINT(m_lWidth),
        CASTUINT(m_lHeight),
        CL_WINDOW,
        &wfe_drawable_vtable,
        (void *)m_pOnscreenDrawable);
        
    m_pOffscreenDrawable = COffscreenDrawable::AllocateOffscreen(hDC, GetPalette(), this);
    
    pOffscreenDrawable = CL_NewDrawable(
        CASTUINT(m_lWidth),
        CASTUINT(m_lHeight),
        CL_BACKING_STORE,
        &wfe_drawable_vtable,
        (void *)m_pOffscreenDrawable);

    GetContext()->compositor = CL_NewCompositor(
        pOnscreenDrawable,
        pOffscreenDrawable,
        m_lOrgX,
        m_lOrgY,
        m_lWidth,
        m_lHeight,
        20);
        
    m_pDrawable = (CDrawable *)m_pOnscreenDrawable;

    //  If a grid cell, select the palette.
    if(IsGridCell() && GetPalette()) {
        ::SelectPalette(hDC, GetPalette(), FALSE);
        int iError = ::RealizePalette(hDC);
    }

    ReleaseContextDC(hDC);
    hDC = NULL;
}

void CPaneCX::SetDrawable(MWContext *pContext, CL_Drawable *pDrawable)
{
    if(pDrawable) {
        CDrawable *pFEDrawable = (CDrawable *)CL_GetDrawableClientData(pDrawable);
        m_pDrawable = pFEDrawable;
    }
    else {
        m_pDrawable = m_pOnscreenDrawable;
    }
}

FE_Region CPaneCX::GetDrawingClip()
{
    if (m_pDrawable) {
        return m_pDrawable->GetClip();
    }
    else {
        return NULL;
    }
}

void CPaneCX::GetDrawingOrigin(int32 *plOrgX, int32 *plOrgY)
{
    if(m_pDrawable)  {
        m_pDrawable->GetOrigin(plOrgX, plOrgY);
    }
    else {
        *plOrgX = *plOrgY = 0;
    }
}

void CPaneCX::RefreshArea(int32 lLeft, int32 lTop, uint32 ulWidth, uint32 ulHeight)
{
    MWContext *pContext = GetContext();

    //  Simple validation, can pass in 0 for all.
    if(ulWidth == 0)    {
        ulWidth = GetDocumentWidth() - lLeft;
    }
    if(ulHeight == 0)   {
        ulHeight = GetDocumentHeight() - lTop;
    }

    XP_Rect rect;
    if(pContext->compositor) {
          rect.left = lLeft - m_lOrgX;
          rect.top = lTop - m_lOrgY;
          rect.right = rect.left + ulWidth;
          rect.bottom = rect.top + ulHeight;
          CL_RefreshWindowRect(pContext->compositor, &rect);
    }

#ifdef DDRAW
    LTRB tempRect(rect.left, rect.top, rect.right, rect.bottom);
    tempRect.left += GetWindowsXPos();
    tempRect.right += GetWindowsXPos();
    tempRect.top += GetWindowsYPos();
    tempRect.bottom += GetWindowsYPos();
    BltToScreen(tempRect, NULL);
#endif
}

//  Window handle is about to go away; let go of it.
void CPaneCX::AftWMDestroy(PaneMessage *pMessage)
{
    SetPane(NULL);
    
    //  If we are supposed to clean up ourselves, do so now.
    if(m_bDestroyOnWMDestroy) {
        DestroyContext();
    }
}

void CPaneCX::PreWMErasebkgnd(PaneMessage *pMessage)
{
    HDC hEraseDC = (HDC)pMessage->wParam;
    HDC hOldSubst = NULL;
    if(hEraseDC != m_hClassDC && hEraseDC != m_hOwnDC) {
        hOldSubst = SubstituteDC(hEraseDC);
    }
    
    //  Layers may want to handle erasing of background.
    MWContext *pContext = GetContext();
    if(pContext->compositor && CL_GetCompositorEnabled(pContext->compositor)) {
        pMessage->bSetRetval = TRUE;
        pMessage->lRetval = (LPARAM)TRUE;
    }
    else {
        RECT rClip;
        ::GetClipBox(hEraseDC, &rClip);
        pMessage->bSetRetval = TRUE;
        pMessage->lRetval = (LPARAM)_EraseBkgnd(hEraseDC, rClip, GetOriginX(), GetOriginY());
    }
    
    if(hEraseDC != m_hClassDC && hEraseDC != m_hOwnDC) {
        HDC hUnSubst = SubstituteDC(hOldSubst);
        //  Someone forget to unsubst their DC?
        ASSERT(hUnSubst == hEraseDC);
    }
}

void CPaneCX::PreWMPaint(PaneMessage *pMessage)
{
    pMessage->lRetval = NULL;
    pMessage->bSetRetval = TRUE;
    
    //  Redirecting output in this fashion not supported yet.
    ASSERT((HDC)pMessage->wParam == NULL);
    
    BOOL bBeginPaint = ::GetUpdateRect(GetPane(), NULL, FALSE);
    if(bBeginPaint) {
        PAINTSTRUCT ps;
        HDC hPaintDC = ::BeginPaint(GetPane(), &ps);
        uint32 ulWidth = ps.rcPaint.right - ps.rcPaint.left;
        uint32 ulHeight = ps.rcPaint.bottom - ps.rcPaint.top;
        if(ulWidth && ulHeight) {
            HDC hOldSubst = NULL;
            if(hPaintDC != m_hClassDC && hPaintDC != m_hOwnDC) {
                hOldSubst = SubstituteDC(hPaintDC);
            }
            RefreshArea(ps.rcPaint.left + GetOriginX(), ps.rcPaint.top + GetOriginY(), ulWidth, ulHeight);
            if(hPaintDC != m_hClassDC && hPaintDC != m_hOwnDC) {
                HDC hUnSubst = SubstituteDC(hOldSubst);
                //  Someone forget to unsubst their DC?
                ASSERT(hUnSubst == hPaintDC);
            }
        }
        ::EndPaint(GetPane(), &ps);
    }
    else {
        //  No update area.
        //  Possible internal draw request (see RedrawWindow).
        //  Not supported until needed.
        ASSERT(0);
    }
}

void CPaneCX::AftWMSize(PaneMessage *pMessage)
{
    UINT uSizeType = (UINT)pMessage->wParam;
    int iWidth = LOWORD(pMessage->lParam);
    int iHeight = HIWORD(pMessage->lParam);
    
    //  Only care if resized visibly.
    if(uSizeType == SIZE_MAXIMIZED || uSizeType == SIZE_RESTORED) {
        BOOL bNiceReload = TRUE;
        
        //  Let frame contexts decide on their own if they would like to reload.
        if(IsFrameContext()) {
            bNiceReload = FALSE;
        }
        
        int iNewWidth = iWidth;
        if(IsVScrollBarOn()) {
            iNewWidth += sysInfo.m_iScrollWidth;
        }
        int iNewHeight = iHeight;
        if(IsHScrollBarOn()) {
            iNewHeight += sysInfo.m_iScrollHeight;
        }
        
        /* Resize the compositor's notion of the window area
         * as well as the size of the background layer. This
         * might all be moot, because we might relayout, but
         * that doesn't happen all the time (e.g. HTML dialogs).
         * We want the composited area - without the scrollbars 
         */
        MWContext *pContext = GetContext();
        if (pContext->compositor) {
            CL_Layer *bglayer, *doclayer;
            CL_Compositor *compositor = pContext->compositor;

            CL_ResizeCompositorWindow(compositor, iNewWidth, iNewHeight);
            doclayer = CL_GetCompositorRoot(compositor);
            if (doclayer) {
                bglayer = CL_GetLayerChildByName(doclayer, LO_BACKGROUND_LAYER_NAME);
                if (bglayer) {
                    XP_Rect bbox;
                    int32 layerWidth, layerHeight;

                    /* Make sure that the new dimensions are larger than the  */
                    /* original ones before resizing the layer. We still want */
                    /* the layer size to be the maximum of the window and the */
                    /* document dimensions.                                   */
                    CL_GetLayerBbox(bglayer, &bbox);
                    layerWidth = bbox.right - bbox.left;
                    layerHeight = bbox.bottom - bbox.top;

                    if (iNewWidth > layerWidth)
                        layerWidth = iNewWidth;
                    if (iNewHeight > layerHeight)
                        layerHeight = iNewHeight;

                    CL_ResizeLayer(bglayer, layerWidth, layerHeight);
                }
            }
        }
        
        m_lWidth = (int32)iNewWidth;
        m_lHeight = (int32)iNewHeight;
        
        //  If we've resized bigger than the page,
        //      and we are scrolled, reset our position.
        if((m_lDocWidth > 0) && (m_lDocWidth < m_lWidth) && (m_lOrgX != 0)) {
            m_lOrgX = 0;
        }
        if((m_lDocHeight > 0) &&(m_lDocHeight < m_lHeight) && (m_lOrgY != 0))   {
            m_lOrgY = 0;
        }

        RealizeScrollBars();
        
        if(bNiceReload) {
#ifdef RELAYOUT_WITHOUT_RELOAD
            LO_RelayoutOnResize(GetDocumentContext(), m_lWidth, m_lHeight, m_lLeftMargin, m_lTopMargin);
#else
            NiceResizeReload();
#endif
        }
    }
}

static void resize_reload_timeout(void *closure)
{
    CPaneCX *cw = VOID2CX(closure, CPaneCX);
    cw->m_pResizeReloadTimeout = NULL;
    cw->NiceReload();
}

//  Wait to do the reload for just a wee bit.
void CPaneCX::NiceResizeReload()
{
    // If there was already a resize timeout that hasn't fired, cancel it,
    // since that means resize messages are still arriving close together
    if(m_pResizeReloadTimeout) {
        FE_ClearTimeout(m_pResizeReloadTimeout);
        m_pResizeReloadTimeout = NULL;
    }
    
    //  Can't reload anything
    if(!CanCreateUrlFromHist()) {
        return;
    }
    
    m_pResizeReloadTimeout = FE_SetTimeout(resize_reload_timeout, this, 200);
}

int CPaneCX::GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoading, BOOL bForceNew)   
{
    // Stop any reload timers, no need.
    if(m_pResizeReloadTimeout)  {
        FE_ClearTimeout(m_pResizeReloadTimeout);
        m_pResizeReloadTimeout = NULL;
    }
    return(CDCCX::GetUrl(pUrl, iFormatOut, bReallyLoading, bForceNew));
}

void CPaneCX::LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)
{
    m_lOrgY = 0;
    m_lOrgX = 0;
    
    // Stop any reload timers, no need.
    if(m_pResizeReloadTimeout)  {
        FE_ClearTimeout(m_pResizeReloadTimeout);
        m_pResizeReloadTimeout = NULL;
    }
    
    //  Call the base.
    CDCCX::LayoutNewDocument(pContext, pURL, pWidth, pHeight, pmWidth, pmHeight);
    
    //  Initialize Scrollbars for new document.
    ShowScrollBars(SB_BOTH, FALSE);
    m_nPageY = SCROLL_UNITS;
    m_nPageX = SCROLL_UNITS;
    
#ifdef XP_WIN32
    SCROLLINFO siY;
    siY.cbSize = sizeof(SCROLLINFO);
    siY.fMask = SIF_PAGE;
    siY.nPage = 0;
    if(GetPane() && IsVScrollBarOn() == TRUE)   {
        ::SetScrollInfo(GetPane(), SB_VERT, &siY, FALSE);
    }
    
    SCROLLINFO siX;
    siX.cbSize = sizeof(SCROLLINFO);
    siX.fMask = SIF_PAGE;
    siX.nPage = 0;
    if(GetPane() && IsHScrollBarOn() == TRUE)   {
        ::SetScrollInfo(GetPane(), SB_HORZ, &siX, FALSE);
    }
#endif
    
    if(*pmWidth || *pmHeight) {
        //  Layout wants to set these.
        m_lLeftMargin = *pmWidth;
        m_lTopMargin = *pmHeight;
        m_lRightMargin = -1 * *pmWidth;
        m_lBottomMargin = -1 * *pmHeight;
    }
    else    {
        //  Set these to the old defaults which I'll never understand.
        m_lLeftMargin = LEFT_MARGIN;
        m_lTopMargin = TOP_MARGIN;
        m_lRightMargin = RIGHT_MARGIN;
        m_lBottomMargin = BOTTOM_MARGIN;
    }
    *pmWidth = m_lLeftMargin;
    *pmHeight = m_lTopMargin;
    
    //  When we report the size to layout, we must always take care to subtract
    //      for the size of the scrollbars if we have dynamic or always on
    //      scrollers.
    if(DynamicScrollBars() == FALSE && IsHScrollBarOn() == FALSE && IsVScrollBarOn() == FALSE)  {
        *pWidth = GetWidth();
        *pHeight = GetHeight();
    }
    else    {
        *pWidth = GetWidth() - sysInfo.m_iScrollWidth;
        *pHeight = GetHeight() - sysInfo.m_iScrollHeight;
    }
}

void CPaneCX::FinishedLayout(MWContext *pContext)
{
    CDCCX::FinishedLayout(pContext);

    //  Have the scroll bars correctly set themselves.
    RealizeScrollBars();
}

void CPaneCX::ShowScrollBars(int iBars, BOOL bShow)  {
    //  Don't do this if we don't have a view.
    if(GetPane() != NULL) {
        //  Turning them off or on?
        if(bShow == FALSE)  {
            //  Decide which set of scrollers, if any, that we need to take
            //      action on, and take that action.
            if(iBars == SB_BOTH) {
                m_bVScrollBarOn = FALSE;
                m_bHScrollBarOn = FALSE;
                ::ShowScrollBar(GetPane(), SB_BOTH, FALSE);
            }
            else if((iBars == SB_VERT) && IsVScrollBarOn()) {
                m_bVScrollBarOn = FALSE;
                ::ShowScrollBar(GetPane(), SB_VERT, FALSE);
            }
            else if((iBars == SB_HORZ) && IsHScrollBarOn()) {
                m_bHScrollBarOn = FALSE;
                ::ShowScrollBar(GetPane(), SB_HORZ, FALSE);
            }
        }
        else    {
            //  Decide which set of scrollers, if any, that we need to take
            //      action on, and take that action.
            if(iBars == SB_BOTH) {
                m_bVScrollBarOn = TRUE;
                m_bHScrollBarOn = TRUE;
                ::ShowScrollBar(GetPane(), SB_BOTH, TRUE);
            }
            else if((iBars == SB_VERT) && !IsVScrollBarOn())    {
                m_bVScrollBarOn = TRUE;
                ::ShowScrollBar(GetPane(), SB_VERT, TRUE);
            }
            else if((iBars == SB_HORZ) && !IsHScrollBarOn())    {
                m_bHScrollBarOn = TRUE;
                ::ShowScrollBar(GetPane(), SB_HORZ, TRUE);
            }
        }
        //  We have just shown/hidden a scroll bar.
        //  Update the window to avoid flash (reduces overall invalidated
        //      rectangle, except when we show/hide both at the same time).
        ::UpdateWindow(GetPane());
    }
}

void CPaneCX::RealizeScrollBars(int32 *pX, int32 *pY)  
{
    if(m_lDocHeight && m_lDocWidth) {
        if(AlwaysShowScrollBars()) {
            ShowScrollBars(SB_BOTH, TRUE);
        }
        
        //  Are we checking for dynamic scroll bars?
        if(DynamicScrollBars() == TRUE) {
            //  If the document fits in our client area, or if we have children, turn them off.
            //  If the document is larger than our client area, turn them on.
            int iSB = -1;
            BOOL bShow = FALSE;
            BOOL vScrollBar = FALSE;
            BOOL hScrollBar = FALSE;
            if (m_lHeight >= m_lDocHeight && m_lWidth >= m_lDocWidth && (IsVScrollBarOn() == TRUE || IsHScrollBarOn() == TRUE)) {
                ShowScrollBars(SB_BOTH, FALSE);
            }
            else if(m_lHeight < m_lDocHeight && m_lWidth < m_lDocWidth && (IsVScrollBarOn() == FALSE || IsHScrollBarOn() == FALSE)) {
                ShowScrollBars(SB_BOTH, TRUE);
            }
            else { 
                if(m_lHeight >= m_lDocHeight && IsVScrollBarOn() == TRUE)   {
                    ShowScrollBars(SB_VERT, FALSE);
                }
                else if(m_lHeight < m_lDocHeight && IsVScrollBarOn() == FALSE)  {
                    ShowScrollBars(SB_VERT, TRUE);
                }
                if(m_lWidth >= m_lDocWidth && IsHScrollBarOn() == TRUE) {
                    ShowScrollBars(SB_HORZ, FALSE);
                }
                else if(m_lWidth < m_lDocWidth && IsHScrollBarOn() == FALSE)    {
                    ShowScrollBars(SB_HORZ, TRUE);
                }
            }
        }

        //  See if we're going to be changing the values.
        BOOL bRefreshHorz = FALSE;
        BOOL bRefreshVert = FALSE;
        if(pX != NULL && m_lOrgX != *pX)   {
            m_lOrgX = *pX;
            bRefreshHorz = TRUE;
        }
        if(pY != NULL && m_lOrgY != *pY)   {
            m_lOrgY = *pY;
            bRefreshVert = TRUE;
        }

        //  Make sure that the current origins are within the document iWidth
        //      and iHeight.
        //  In the event that they are exactly the same, make sure to refresh
        //      the scroll bar anyhow (this will happen if you scroll to the
        //      bottom of a page, go to a new page, and then go back; your scroll
        //      bars are wrong).
        int32 lCalcHeight = m_lDocHeight - m_lHeight + (IsHScrollBarOn() ? sysInfo.m_iScrollHeight : 0);
        if(m_lOrgY >= lCalcHeight && lCalcHeight > 0)  {
            m_lOrgY = lCalcHeight;  // always show one screen
            bRefreshVert = TRUE;
        }
        int32 lCalcWidth = m_lDocWidth - m_lWidth + (IsVScrollBarOn() ? sysInfo.m_iScrollWidth : 0);
        if(m_lOrgX >= lCalcWidth && lCalcWidth > 0) {
            m_lOrgX = lCalcWidth;    // always leave some visible
            bRefreshHorz = TRUE;
        }

    #ifdef XP_WIN32
        //  Special fun for proportional scrollbars.
        SCROLLINFO siY;
        siY.cbSize = sizeof(SCROLLINFO);
        siY.fMask = SIF_PAGE;
        //  If the document is longer than a single page
        if(m_lDocHeight > m_lHeight && IsVScrollBarOn() == TRUE)   {
            siY.nPage = m_lHeight * SCROLL_UNITS / m_lDocHeight;
            if(GetPane())  {
                ::SetScrollInfo(GetPane(), SB_VERT, &siY, FALSE);
                if((UINT)m_nPageY != SCROLL_UNITS - siY.nPage)    {
                    m_nPageY = SCROLL_UNITS - siY.nPage;
                    bRefreshVert = TRUE;
                }
            } else
                bRefreshVert = TRUE;
        }
        else if(IsVScrollBarOn() == TRUE)    {
            siY.nPage = SCROLL_UNITS;
            if(GetPane())  {
                ::SetScrollInfo(GetPane(), SB_VERT, &siY, FALSE);
                m_nPageY = SCROLL_UNITS;
            }
        }

        SCROLLINFO siX;
        siX.cbSize = sizeof(SCROLLINFO);
        siX.fMask = SIF_PAGE;
        //  If the document is wider than a single screen
        if((m_lDocWidth != 0) && (m_lDocWidth > m_lWidth) && IsHScrollBarOn() == TRUE) {
            siX.nPage = m_lWidth * SCROLL_UNITS / m_lDocWidth;
            if(GetPane())  {
                ::SetScrollInfo(GetPane(), SB_HORZ, &siX, FALSE);
                if((UINT)m_nPageX != SCROLL_UNITS - siX.nPage)    {
                    m_nPageX = SCROLL_UNITS - siX.nPage;
                    bRefreshHorz = TRUE;
                }
            } else
                bRefreshHorz = TRUE;
        }
        else if(IsHScrollBarOn() == TRUE)    {
            siX.nPage = SCROLL_UNITS;
            if(GetPane())  {
                ::SetScrollInfo(GetPane(), SB_HORZ, &siX, FALSE);
                m_nPageX = SCROLL_UNITS;
            }
        }
    #endif

        //  Figure the thumb position.
        float fYPos = 0.0f;
        float fXPos = 0.0f;
        if(m_lDocHeight != 0)    {
            long lScrollHeight = m_lDocHeight - m_lHeight;
            if(lScrollHeight != 0)
                fYPos = (float)m_lOrgY / (float) lScrollHeight;
            else
                fYPos = 1.0f;
        }
        if(m_lDocWidth != 0) {
            long lScrollWidth = m_lDocWidth - m_lWidth;
            if(lScrollWidth != 0)
                fXPos = (float)m_lOrgX / (float) lScrollWidth;
            else
                fXPos = 1.0f;
        }

        //  See if we should turn off the scroll bars.

        if(m_lDocWidth <= m_lWidth && IsHScrollBarOn() == TRUE && GetPane()) {
            ::EnableScrollBar(GetPane(), SB_HORZ, ESB_DISABLE_BOTH);
        }
        else if(IsHScrollBarOn() == TRUE && GetPane())    {
            ::EnableScrollBar(GetPane(), SB_HORZ, ESB_ENABLE_BOTH);

            // only reset if different to avoid flashing on NT and Win16
            int iNewPosX = (int) (fXPos * m_nPageX);
            if(iNewPosX != ::GetScrollPos(GetPane(), SB_HORZ) || bRefreshHorz == TRUE)  {
                ::SetScrollPos(GetPane(), SB_HORZ, iNewPosX, TRUE);    
            }
        }


        if(m_lDocHeight <= m_lHeight && IsVScrollBarOn() == TRUE && GetPane())   {
            ::EnableScrollBar(GetPane(), SB_VERT, ESB_DISABLE_BOTH);
        }
        else if(IsVScrollBarOn() == TRUE && GetPane())    {
            ::EnableScrollBar(GetPane(), SB_VERT, ESB_ENABLE_BOTH);

            // only reset if different to avoid flashing on NT and Win16
            int iNewPosY = (int) (fYPos * m_nPageY);
            if(iNewPosY != ::GetScrollPos(GetPane(), SB_VERT) || bRefreshVert == TRUE)  {
                ::SetScrollPos(GetPane(), SB_VERT, iNewPosY, TRUE);
            }
        }

        MWContext *pContext = GetContext();
        if(pContext->compositor) {
            CL_ScrollCompositorWindow(pContext->compositor, m_lOrgX, m_lOrgY);
        }
    }
}

void CPaneCX::SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength)   {
    //  Call the base.
    CDCCX::SetDocDimension(pContext, iLocation, lWidth, lLength);
    RealizeScrollBars();
}

void CPaneCX::PreWMVScroll(PaneMessage *pMsg)
{
#ifdef XP_WIN16
    UINT uSBCode = (UINT)pMsg->wParam;
    UINT uPos = LOWORD(pMsg->lParam);
    HWND hwndCtrl = (HWND)HIWORD(pMsg->lParam);
#else
    UINT uSBCode = LOWORD(pMsg->wParam);
    UINT uPos = HIWORD(pMsg->wParam);
    HWND hwndCtrl = (HWND)pMsg->lParam;
#endif
    
    Scroll(SB_VERT, uSBCode, uPos, hwndCtrl);
    
    pMsg->lRetval = NULL;
    pMsg->bSetRetval = TRUE;
}

void CPaneCX::PreWMHScroll(PaneMessage *pMsg)
{
#ifdef XP_WIN16
    UINT uSBCode = (UINT)pMsg->wParam;
    UINT uPos = LOWORD(pMsg->lParam);
    HWND hwndCtrl = (HWND)HIWORD(pMsg->lParam);
#else
    UINT uSBCode = LOWORD(pMsg->wParam);
    UINT uPos = HIWORD(pMsg->wParam);
    HWND hwndCtrl = (HWND)pMsg->lParam;
#endif
    
    Scroll(SB_HORZ, uSBCode, uPos, hwndCtrl);
    
    pMsg->lRetval = NULL;
    pMsg->bSetRetval = TRUE;
}

void CPaneCX::AftWMMouseActivate(PaneMessage *pMsg)
{
    BOOL bSetFocus = FALSE;
    if(pMsg->bSetRetval) {
        //  Check to see if we need to activate.
        if(pMsg->lRetval == MA_ACTIVATE || pMsg->lRetval == MA_ACTIVATEANDEAT) {
            bSetFocus = TRUE;
        }
    }
    else {
        //  Real class didn't handle, take some action.
        pMsg->bSetRetval = TRUE;
        pMsg->lRetval = MA_ACTIVATE;
        bSetFocus = TRUE;
    }
    
    if(bSetFocus) {
        if(::GetFocus() != GetPane()) {
            ::SetFocus(GetPane());
        }
    }
}

#if defined(XP_WIN32) && _MSC_VER >= 1100
void CPaneCX::PreWMMouseWheel(PaneMessage *pMsg)
{
    //  Increase the delta.
    m_iWheelDelta += MOUSEWHEEL_DELTA(pMsg->wParam, pMsg->lParam);

    //  Number of lines to scroll.
    UINT uScroll = intelli.ScrollLines();

    //  Direction.
    BOOL bForward = TRUE;
    if(m_iWheelDelta < 0)   {
        bForward = FALSE;
    }

    //  Scroll bar code to use.
    UINT uSBCode = SB_LINEUP;

    if(m_iWheelDelta / WHEEL_DELTA)  {
        if(uScroll == WHEEL_PAGESCROLL)   {
            if(bForward)    {
                uSBCode = SB_PAGEUP;
            }
            else    {
                uSBCode = SB_PAGEDOWN;
            }
            uScroll = 1;
        }
        else    {
            if(bForward)    {
                uSBCode = SB_LINEUP;
            }
            else    {
                uSBCode = SB_LINEDOWN;
            }
        }

        //  Take off scroll increment.
        UINT uLoops = 0;
        while(m_iWheelDelta / WHEEL_DELTA)  {
            if(bForward)   {
                m_iWheelDelta -= WHEEL_DELTA;
            }
            else    {
                m_iWheelDelta += WHEEL_DELTA;
            }
            uLoops++;
        }

        //  Do it.
        if(uLoops) {
            Scroll(SB_VERT, uSBCode, 0, NULL, uScroll * uLoops);
        }
    }

    pMsg->lRetval = (LPARAM)1;
    pMsg->bSetRetval = TRUE;
}

void CPaneCX::PreWMHackedMouseWheel(PaneMessage *pMsg)
{
    //  Shunt.
    WPARAM wSubst = pMsg->wParam;
    pMsg->wParam = wSubst << 16;
    PreWMMouseWheel(pMsg);
    pMsg->wParam = wSubst;
}
#endif

void CPaneCX::Scroll(int iBars, UINT uSBCode, UINT uPos, HWND hCtrl, UINT uTimes)
{
    //  Provide a way to keep flow of control but not return early.
    BOOL bContinue = TRUE;

    if((iBars == SB_VERT || iBars == SB_BOTH) && IsVScrollBarOn() && GetHeight() < GetDocumentHeight()) {
        //  Calc an area leaving at least one page.
        //  Align the pixel and twips boundary, and check again to see if we're really
        //      doing anything.
        //  Account for Horiz scroll bar getting in the way.
        int32 lScrollable = GetDocumentHeight() - GetHeight();
        if(IsHScrollBarOn()) {
            lScrollable += sysInfo.m_iScrollHeight;
        }
        if(lScrollable > 0)    {
            int32 lOldOrgY = GetOriginY();
            int32 lScrollTwips = 0;
            double fYPos = 0.0f;
            int32 lTrack = 0;

            //  Figure out what we're doing.
            switch(uSBCode) {
            case SB_PAGEUP:
                lScrollTwips = (-1 * GetHeight()) + VSCROLL_LINE;
                break;
            case SB_PAGEDOWN:
                lScrollTwips = GetHeight() - VSCROLL_LINE;
                break;
            case SB_LINEUP:
                lScrollTwips = -1 * VSCROLL_LINE;
                break;
            case SB_LINEDOWN:
                lScrollTwips = VSCROLL_LINE;
                break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
                //  User is dragging the thumb tack.
                //  Only move if we've moved more than a single pixel, or the display
				//  gets messed up
                fYPos = (double)uPos / (double)GetPageY();
                lTrack = (int32)((fYPos * lScrollable)+0.5);
                break;
            case SB_TOP:
                lScrollTwips = -1 * GetOriginY();
                break;
            case SB_BOTTOM:
                lScrollTwips = lScrollable - GetOriginY();
                break;
            default:
                bContinue = FALSE;
                break;
            }
            
            if(bContinue) {
                //  Check to see if we need to do further math....
                if(uSBCode != SB_THUMBPOSITION && uSBCode != SB_THUMBTRACK) {
                    lTrack = GetOriginY();
                    lTrack += lScrollTwips * (int32)uTimes;
                    
                    if (lTrack < 0)
                        lTrack = 0;
                    else if(lTrack > lScrollable)
                        lTrack = lScrollable;
                }
                
                //  Position the scroll bar on an even pixel boundary
                RealizeScrollBars(NULL, &lTrack);
                
                //  Figure the distance we actually scrolled.
                int32 lPixAct = lOldOrgY - GetOriginY();
                if(lPixAct)    {
                    ::ScrollWindowEx(GetPane(), 0, (int) lPixAct, NULL, NULL, NULL, NULL, SW_INVALIDATE |
                        SW_ERASE | SW_SCROLLCHILDREN);
                    ::UpdateWindow(GetPane());
                }
            }
        }
    }
    
    //  Reset for horizontal test.
    bContinue = TRUE;
    
    if((iBars == SB_HORZ || iBars == SB_BOTH) && IsHScrollBarOn() && GetWidth() < GetDocumentWidth()) {
        //  Calc an area leaving at least one page.
        //  Align the pixel and twips boundary, and check again to see if we're really
        //      doing anything.
        //  Account for vertical scroller getting in the way.
        int32 lScrollable = GetDocumentWidth() - GetWidth();
        if(IsVScrollBarOn()) {
            lScrollable += sysInfo.m_iScrollWidth;
        }
        if(lScrollable > 0)    {
            int32 lOldOrgX = GetOriginX();
            int32 lScrollTwips = 0;
            double fXPos = 0.0f;
            int32 lTrack = 0;
            
            //  Figure out what we're doing.
            switch(uSBCode) {
            case SB_PAGELEFT:
                lScrollTwips = -1 * GetWidth();
                break;
            case SB_PAGERIGHT:
                lScrollTwips = GetWidth();
                break;
            case SB_LINELEFT:
            case SB_LINERIGHT:
                lScrollTwips = HSCROLL_LINE;
                if(uSBCode == SB_LINELEFT)  {
                    lScrollTwips *= -1;
                }
                break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
                //  User is dragging the thumb tack.
                //  Only move if we've moved more than a single pixel, or the display
				//  gets messed up.
                fXPos = (double)uPos / (double)GetPageX();
                lTrack = (int32)((fXPos * lScrollable)+0.5);
                break;
            default:
                bContinue = FALSE;
                break;
            }
            
            if(bContinue) {
                //  Check to see if we need to do further math....
                if(uSBCode != SB_THUMBPOSITION && uSBCode != SB_THUMBTRACK) {
                    lTrack = GetOriginX();
                    lTrack += lScrollTwips * (int32)uTimes;
                    
                    if(lTrack > lScrollable)
                        lTrack = lScrollable;
                    if(lTrack < 0)
                        lTrack = 0;
                }
                
                //  Position the scroll bar.
                RealizeScrollBars(&lTrack);
                
                //  Figure the distance we actually scrolled.
                int32 lPixAct = lOldOrgX - GetOriginX();
                if(lPixAct)    {
                    ::ScrollWindowEx(GetPane(), (int) lPixAct, 0, NULL, NULL, NULL, NULL, SW_INVALIDATE |
                    SW_ERASE | SW_SCROLLCHILDREN);
                    ::UpdateWindow(GetPane());
                }
            }
        }
    }
}

void CPaneCX::GetWindowOffset(int32 *pX, int32 *pY)
{
    RECT rWindow;
    ::GetWindowRect(GetPane(), &rWindow);
    
    *pX = rWindow.left;
    *pY = rWindow.top;
    return;
}

void CPaneCX::MakeElementVisible(int32 lX, int32 lY)
{
    //  Figure out if any element is there.
    XY Point;
    Point.x = lX;
    Point.y = lY;

    LO_Element *pElement = GetLayoutElement(Point, NULL);
    if(pElement == NULL)    {
        SetDocPosition(GetContext(), FE_VIEW, lX, lY);
        return;
    }

    //  Figure up the coords of the element.
    LTRB Rect;
    LO_Any *pAny = &(pElement->lo_any);
    Rect.left = pAny->x + pAny->x_offset;
    Rect.top = pAny->y + pAny->y_offset;
    Rect.right = Rect.left + pAny->width;
    Rect.bottom = Rect.top + pAny->height;

    //  Is it currently Fully On Screen?
    //  Disregard the Y values (we'll want to scroll exactly on the Y, but perhaps not on the X).
    //  To the right?
    if(Rect.left >= GetOriginX())   {
        //  To the left?
        if(Rect.right <= GetOriginX() + GetWidth()) {
            //  It's on the X view.
            //  Use the current X origin.
            lX = GetOriginX();
            SetDocPosition(GetContext(), FE_VIEW, lX, lY);
            return;
        }
    }

    //  X wasn't on screen fully, so we're going to use very exact values when scrolling
    //      for whatever effect this gives....
    SetDocPosition(GetContext(), FE_VIEW, lX, lY);
}

void CPaneCX::PreNavCenterQueryPosition(PaneMessage *pMessage)
{
    //  Only handle if we're a NavCenter HTML Pane.
    if(IsNavCenterHTMLPane()) {
        NAVCENTPOS *pPos = (NAVCENTPOS *)pMessage->lParam;
        
        //  We like being at the bottom.
        pPos->m_iYDisposition = INT_MAX;
        
        //  We like being this many units in size.
        pPos->m_iYVector = 100;
        
        //  Handled.
        pMessage->lRetval = NULL;
        pMessage->bSetRetval = TRUE;
    }
}

void CPaneCX::PreIdleUpdateCmdUI(PaneMessage *pMsg)
{
    //  Don't want to update CMD UI unless we have a frame parent.
    //  This effectively stops CMD UI in the NavCenter HTML pane
    //      from messing with the UI state when docked.
    if(IsNavCenterHTMLPane()) {
        //  Handled.
        pMsg->lRetval = NULL;
        pMsg->bSetRetval = TRUE;
    }
}

#ifdef XP_WIN16
HINSTANCE CPaneCX::GetSegment()
{
    //  Form elements on a per context basis receive their own segment.
    HINSTANCE hRetval = NULL;
    
    if(NULL == m_hTextElementSegment) {
        m_hTextElementSegment = ::GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, (UINT) 8 * 1024); 
    }
    if(m_hTextElementSegment && NULL == m_lpTextElementHeap) {
        //  Initialize the segment.
        m_lpTextElementHeap = ::GlobalLock(m_hTextElementSegment);
        if(m_lpTextElementHeap) {
            ::LocalInit(HIWORD((LONG)m_lpTextElementHeap), 0, (WORD)(::GlobalSize(m_hTextElementSegment) - 16));
            ::UnlockSegment(HIWORD((LONG)m_lpTextElementHeap));
        }
    }
    
    if(m_lpTextElementHeap) {
        hRetval = (HINSTANCE)HIWORD((LONG)m_lpTextElementHeap);
    }
    
    return(hRetval);
}
#endif








LRESULT
CALLBACK
#ifndef _WIN32
_export
#endif
PaneProc(HWND hPane, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef XP_WIN32
    const char *pPropName = STR_CPANECX;  //  Not a UI string.
#else
    const char *pPropNameHi = STR_CPANECX_HI;
    const char *pPropNameLo = STR_CPANECX_LO;
#endif
    PaneMessage message(wParam, lParam);
    
    //  Get the property (which is a this pointer).
    void *pvThis = NULL;
#ifdef XP_WIN32
    pvThis = (void *)GetProp(hPane, pPropName);
#else
    pvThis = (void *)MAKELONG(GetProp(hPane, pPropNameLo), GetProp(hPane, pPropNameHi));
#endif
    if(pvThis) {
        CPaneCX *pThis = VOID2CX(pvThis, CPaneCX);
        
        //  Messages handled BEFORE calling other window procedure.
        //  Make calls here when you don't care about the LRESULT
        //      of the normal message handler or don't want the normal
        //      message handler being called at all (by marking that
        //      the return value has been set).
        switch(uMsg) {
            case WM_PAINT:
                pThis->PreWMPaint(&message);
                break;
            case WM_ERASEBKGND:
                pThis->PreWMErasebkgnd(&message);
                break;
            case WM_VSCROLL:
                pThis->PreWMVScroll(&message);
                break;
            case WM_HSCROLL:
                pThis->PreWMHScroll(&message);
                break;
            case WM_NAVCENTER_QUERYPOSITION:
                pThis->PreNavCenterQueryPosition(&message);
                break;
            case WM_IDLEUPDATECMDUI:
                pThis->PreIdleUpdateCmdUI(&message);
                break;
#if defined(XP_WIN32) && _MSC_VER >= 1100
            case WM_MOUSEWHEEL:
                pThis->PreWMMouseWheel(&message);
                break;
#endif
            default:
                //  Handle non constant messages here.
#if defined(XP_WIN32) && _MSC_VER >= 1100
                if(uMsg == msg_MouseWheel)   {
                    pThis->PreWMHackedMouseWheel(&message);
                }
#endif
                break;
        }
        
        //  Call previous window proc.
        if(!message.bSetRetval) {
            WNDPROC pPrevProc = pThis->GetSubclassedProc();
            if(pPrevProc) {
                message.lRetval = CallWindowProc(pPrevProc, hPane, uMsg, wParam, lParam);
                message.bSetRetval = TRUE;
            }
        }
        
        //  Notifications handled AFTER calling other window procedure.
        //  Make calls here when you care about the LRESULT
        //      of any above message handler, or want to handle a
        //      message if no one else did before the default window
        //      procedure is invoked..
        switch(uMsg) {
            case WM_SIZE:
                pThis->AftWMSize(&message);
                break;
            case WM_DESTROY:
                pThis->AftWMDestroy(&message);
                break;
            case WM_MOUSEACTIVATE:
                pThis->AftWMMouseActivate(&message);
                break;
            default:
                break;
        }
        
        //  Default handler if nothing happened.
        if(!message.bSetRetval) {
            message.lRetval = DefWindowProc(hPane, uMsg, wParam, lParam);
            message.bSetRetval = TRUE;
        }
    }
    else {
        //  Either someone subclassed and we need to unsubclass in
        //      their stack, or this is getting called before we
        //      set the property on the window.
        //  Don't crash or do anything stupid, but we currently don't
        //      attempt to handle.
        message.lRetval = DefWindowProc(hPane, uMsg, wParam, lParam);
        message.bSetRetval = TRUE;
    }
    
    ASSERT(message.bSetRetval);
    return(message.lRetval);
}

//  Subclass or unsubclass a window.
BOOL CPaneCX::SubClass(HWND hWnd, BOOL bSubClass)
{
    BOOL bRetval = FALSE;
#ifdef XP_WIN32
    const char *pPropName = STR_CPANECX;  //  Not a UI string.
#else
    const char *pPropNameHi = STR_CPANECX_HI;  //  Not a UI string.
    const char *pPropNameLo = STR_CPANECX_LO;  //  Not a UI string.
#endif
    
    if(hWnd) {
        if(bSubClass) {
            ASSERT(!m_pPrevProc);
            
            //  Make sure no one else is already doing this window.
            //  We could add the support by making each property
            //      name unique by pid() then ptr, but no need right now.
            if(!
#ifdef XP_WIN32
                (::GetProp(hWnd, pPropName))
#else
                (::GetProp(hWnd, pPropNameLo) || ::GetProp(hWnd, pPropNameHi))
#endif
                ) {
                m_pPrevProc = (WNDPROC)::SetWindowLong(hWnd, GWL_WNDPROC, (LONG)PaneProc);
                if(m_pPrevProc) {
                    BOOL bAddProp;
#ifdef XP_WIN32
                    bAddProp = ::SetProp(hWnd, pPropName, (HANDLE)this);
#else
                    bAddProp = ::SetProp(hWnd, pPropNameHi, (HANDLE)HIWORD(this));
                    if(bAddProp)    {
                        bAddProp = ::SetProp(hWnd, pPropNameLo, (HANDLE)LOWORD(this));
                    }
#endif
                    if(bAddProp) {
                        bRetval = TRUE;
                    }
                    else {
                        LONG lCheck = ::SetWindowLong(hWnd, GWL_WNDPROC, (LONG)m_pPrevProc);
                        m_pPrevProc = NULL;
                        ASSERT(lCheck == (LONG)PaneProc);
                    }
                }
            }
        }
        else {
            ASSERT(m_pPrevProc);
            
            void *pThis = NULL;
#ifdef XP_WIN32
            pThis = (void *)::RemoveProp(hWnd, pPropName);
#else
            pThis = (void *)MAKELONG(::RemoveProp(hWnd, pPropNameLo), ::RemoveProp(hWnd, pPropNameHi));
#endif
            ASSERT(pThis && pThis == (void *)this);
            if(pThis && pThis == (void *)this) {
                LONG lOurProc = ::GetWindowLong(hWnd, GWL_WNDPROC);
                ASSERT(lOurProc && lOurProc == (LONG)PaneProc);
                if(lOurProc && lOurProc == (LONG)PaneProc) {
                    LONG lCheck = ::SetWindowLong(hWnd, GWL_WNDPROC, (LONG)m_pPrevProc);
                    ASSERT(lCheck && lCheck == (LONG)PaneProc);
                    if(lCheck && lCheck == (LONG)PaneProc) {
                        bRetval = TRUE;
                    }
                }
                m_pPrevProc = NULL;
            }
        }
    }
    
    return(bRetval);
}

