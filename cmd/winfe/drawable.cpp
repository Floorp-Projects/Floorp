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

//   drawable.cpp - Classes used for maintaining the notion of 
//                  offscreen and onscreen drawables, so that 
//                  document contents can be drawn to either
//                  location. Currently used for LAYERS.

#include "stdafx.h"

#ifdef LAYERS
#include "drawable.h"
#include "xp_rect.h"
#include "cxprint.h"

// In this implementation, we only have a single offscreen drawable,
// and all its state is stored in the following statics:
HDC COffscreenDrawable::m_hDC = 0;
HPALETTE COffscreenDrawable::m_hSelectedPalette = 0;
COffscreenDrawable *COffscreenDrawable::m_pOffscreenDrawable = NULL;
HBITMAP COffscreenDrawable::m_hBitmap = 0;
CL_Drawable * COffscreenDrawable::m_pOwner = NULL;
int32 COffscreenDrawable::m_lWidth = 0;
int32 COffscreenDrawable::m_lHeight = 0;
UINT COffscreenDrawable::m_uRefCnt = 0;

// CDrawable base class
CDrawable::CDrawable() 
{
    m_lOrgX = m_lOrgY = 0;
    m_hClipRgn = FE_NULL_REGION;
	m_parentContext = NULL;
}
CDrawable::CDrawable(CAbstractCX*	parentContext) 
{
    m_lOrgX = m_lOrgY = 0;
    m_hClipRgn = FE_NULL_REGION;
	m_parentContext = parentContext;
}

// Set and get the origin of the drawable 
void 
CDrawable::SetOrigin(int32 lOrgX, int32 lOrgY)
{
    m_lOrgX = lOrgX;
    m_lOrgY = lOrgY;
}

void 
CDrawable::GetOrigin(int32 *lOrgX, int32 *lOrgY)
{
    *lOrgX = m_lOrgX;
    *lOrgY = m_lOrgY;
}

// Set the clip region of the drawable.
void 
CDrawable::SetClip(FE_Region hClipRgn)
{
	HDC hDC = GetDrawableDC();
	ASSERT(hDC);
	if (hDC) {
		if (hClipRgn == FE_NULL_REGION)
			::SelectClipRgn(hDC, NULL);
		else
			::SelectClipRgn(hDC, FE_GetMDRegion(hClipRgn));
			m_hClipRgn = hClipRgn;
	}
	ReleaseDrawableDC(hDC);
}

typedef struct fe_CopyRectStruct {
    HDC hSrcDC;
    HDC hDstDC;
	CAbstractCX*	m_parentContext;
} fe_CopyRectStruct;

// For copying pixels, we draw a rectangle at a time
static void
fe_copy_rect_func(void *pRectStruct, XP_Rect *pRect)
{
#ifdef DDRAW	
	CAbstractCX* m_parentContext = ((fe_CopyRectStruct *)pRectStruct)->m_parentContext;
	if (m_parentContext && m_parentContext->IsWindowContext()) {
		CWinCX* pWinCX =  (CWinCX*)m_parentContext;
		if ( pWinCX->GetPrimarySurface()) {
			LTRB rect;
			rect.left = pRect->left;
			rect.top = pRect->top;
			rect.right = pRect->right;
			rect.bottom = pRect->bottom;
			pWinCX->BltToScreen(rect);
			return;
		}
	}
	 
#endif
	HDC hSrcDC = ((fe_CopyRectStruct *)pRectStruct)->hSrcDC;
	HDC hDstDC = ((fe_CopyRectStruct *)pRectStruct)->hDstDC;

	BitBlt(hDstDC, CASTINT(pRect->left), CASTINT(pRect->top), 
		   CASTINT(pRect->right-pRect->left),
		   CASTINT(pRect->bottom-pRect->top),
		   hSrcDC,
		   CASTINT(pRect->left), CASTINT(pRect->top),
		   SRCCOPY);
}

void
CDrawable::CopyPixels(CDrawable *pSrcDrawable, FE_Region hCopyRgn)
{
    HDC hSrcDC;
    HDC hDstDC = GetDrawableDC();
    fe_CopyRectStruct rectStruct;

    ASSERT(hDstDC);
    
    if (hDstDC && pSrcDrawable && (hCopyRgn != FE_NULL_REGION)) {
		hSrcDC = pSrcDrawable->GetDrawableDC();

		if (hSrcDC) {
	#ifndef XP_WIN32
				// On Win16, we must BLT through the clip, instead of iterating
				// through the region's rectangles, due to Win16's inability
				// to decompose a region into its component rectangles.
				SetClip(hCopyRgn);
	#endif
			rectStruct.hDstDC = hDstDC;
			rectStruct.hSrcDC = hSrcDC;
			rectStruct.m_parentContext = m_parentContext;
				// The following statement is misleading, because on
				// Win16, FE_ForEachRectInRegion() calls its client
				// function only once, using the bounding box of the
				// region, not with each individual rectangle in the
				// region.
			FE_ForEachRectInRegion(hCopyRgn, 
					   (FE_RectInRegionFunc)fe_copy_rect_func,
					   (void *)&rectStruct);
	#ifndef XP_WIN32
				SetClip(NULL);
	#endif

			ReleaseDrawableDC(hSrcDC);
		}
		ReleaseDrawableDC(hDstDC);
    }
}


COnscreenDrawable::COnscreenDrawable(HDC hDC, CAbstractCX* parentContext)
:CDrawable(parentContext)
{
    m_hDC = hDC;
}

#ifndef MOZ_NGLAYOUT
CPrinterDrawable::CPrinterDrawable(HDC hDC, 
                                   int32 lLeftMargin, 
                                   int32 lRightMargin,
                                   int32 lTopMargin,
                                   int32 lBottomMargin,
								   CAbstractCX* parentContext)
{
    m_hDC = hDC;
    m_lLeftMargin = lLeftMargin;
    m_lRightMargin = lRightMargin;
    m_lTopMargin = lTopMargin;
    m_lBottomMargin = lBottomMargin;
	m_parentContext = parentContext;
}

void 
CPrinterDrawable::SetClip(FE_Region hClipRgn)
{
    XP_Rect bbox;

    if (hClipRgn != FE_NULL_REGION) {
        FE_Region hTempClipRgn;
        RECT rect;
        
        /* Get region bounding box and add in the margins */
        FE_GetRegionBoundingBox(hClipRgn, &bbox);
        XP_OffsetRect(&bbox, m_lLeftMargin, m_lTopMargin);

        rect.left = bbox.left;
        rect.right = bbox.right;
        rect.top = bbox.top;
        rect.bottom = bbox.bottom;
        
        ::LPtoDP(m_hDC, (LPPOINT)&rect, 2);
        
        bbox.left = rect.left;
        bbox.right = rect.right;
        bbox.top = rect.top;
        bbox.bottom = rect.bottom;

        /* 
         * Create, set and then destroy a region based on the offset and
         * scaled bounding box.
         */
        hTempClipRgn = FE_CreateRectRegion(&bbox);
        
        CDrawable::SetClip(hTempClipRgn);
        if (hTempClipRgn)
            FE_DestroyRegion(hTempClipRgn);
#ifdef XP_WIN32        
		CPrintCX* pContext = (CPrintCX*)m_parentContext;
		HDC m_offscrnDC = pContext->GetOffscreenDC();

		if (m_offscrnDC) {
			FE_GetRegionBoundingBox(hClipRgn, &bbox);
			CPrintCX* prntContext = (CPrintCX*)m_parentContext;
			bbox.left = (bbox.left +prntContext->GetXConvertUnit())/ prntContext->GetXConvertUnit();
			bbox.right = (bbox.right +prntContext->GetYConvertUnit())/ prntContext->GetXConvertUnit();
			bbox.top = (bbox.top + prntContext->GetXConvertUnit())/ prntContext->GetYConvertUnit();
			bbox.bottom = (bbox.bottom + prntContext->GetYConvertUnit()) / prntContext->GetYConvertUnit();
	        hTempClipRgn = FE_CreateRectRegion(&bbox);
			if (hTempClipRgn == FE_NULL_REGION)
				::SelectClipRgn(m_offscrnDC, NULL);
			else
				::SelectClipRgn(m_offscrnDC, FE_GetMDRegion(hTempClipRgn));
			if (hTempClipRgn)
				FE_DestroyRegion(hTempClipRgn);
		}
#endif        
    }
    else
        CDrawable::SetClip(FE_NULL_REGION);
}
#endif /* MOZ_NGLAYOUT */


// We should get rid of this function, since it does nothing
// that the new operator does not.
COffscreenDrawable *
COffscreenDrawable::AllocateOffscreen(HDC hParentDC, HPALETTE hPal, CAbstractCX* parentContext)
{
    return new COffscreenDrawable(hParentDC, hPal, parentContext);
}

COffscreenDrawable::COffscreenDrawable(HDC hParentDC, HPALETTE hPal, CAbstractCX* parentContext)
:CDrawable(parentContext)
{
    m_hParentDC = hParentDC;
    m_hSaveBitmap = NULL;
    m_hSavePalette = NULL;
    m_hParentPal = hPal;
	m_parentContext = parentContext;
}
// Set the clip region of the drawable.
void 
COffscreenDrawable::SetClip(FE_Region hClipRgn)
{
#ifdef DDRAW
	if (m_parentContext && m_parentContext->IsWindowContext()){
		CWinCX *pwincx = (CWinCX*)m_parentContext;
		if (pwincx->GetBackSurface()) {
			pwincx->SetClipOnDrawSurface(pwincx->GetBackSurface(), FE_GetMDRegion(hClipRgn));
		}
	}
	else 
		CDrawable::SetClip(hClipRgn);
#else
	CDrawable::SetClip(hClipRgn);
#endif
	m_hClipRgn = hClipRgn;
}
HDC COffscreenDrawable::GetDrawableDC()
{
    if (m_hDC) 
		return m_hDC;
	else {	// we must be using DirectDraw's offscreen surface.
		CWinCX *pwincx = (CWinCX*)m_parentContext;

		return pwincx->GetDispDC();
	}
}
void COffscreenDrawable::ReleaseDrawableDC(HDC hdc) 
{
}

// Get rid of any offscreen constructs if they exist
void
COffscreenDrawable::delete_offscreen()
{
    if (m_hDC) {
	::SelectObject(m_hDC, m_hSaveBitmap);
        ::SelectPalette(m_hDC, (HPALETTE)GetStockObject(DEFAULT_PALETTE), FALSE);
        m_hSelectedPalette = 0;
	DeleteDC(m_hDC);
    }

    if (m_hBitmap) 
       ::DeleteObject(m_hBitmap);
    
    m_hDC = NULL;
    m_hSaveBitmap = NULL;
    m_hSavePalette = NULL;
    m_hBitmap = NULL;
    m_lWidth = m_lHeight = 0;
}

COffscreenDrawable::~COffscreenDrawable()
{
    // It's possible that the window that "owns" this offscreen
    // drawable is about to be deleted, along with its palette.  We
    // have to deselect the palette from this drawable's DC so that
    // it can be safely deleted.
    if (m_hSelectedPalette && (m_hParentPal == m_hSelectedPalette)) {
        ::SelectPalette(m_hDC, (HPALETTE)GetStockObject(DEFAULT_PALETTE), FALSE);
        m_hSelectedPalette = 0;
    }
}

void 
COffscreenDrawable::InitDrawable(CL_Drawable *pCLDrawable)
{
    m_uRefCnt++;
}

void 
COffscreenDrawable::RelinquishDrawable(CL_Drawable *pCLDrawable)
{
    ASSERT(m_uRefCnt > 0);

    if (m_uRefCnt)
        m_uRefCnt--;

    // There are no clients left. Let's get rid of the offscreen
    // bitmap.
    if (m_uRefCnt == 0)
	delete_offscreen();
}

// Called before using the drawable.
PRBool 
COffscreenDrawable::LockDrawable(CL_Drawable *pCLDrawable, 
				 CL_DrawableState newState)
{

    if (newState == CL_UNLOCK_DRAWABLE)
        return PR_TRUE;

	CWinCX* pWinCX =  (CWinCX*)m_parentContext;
#ifdef DDRAW
    if (!pWinCX->GetBackSurface() && !m_hBitmap)
        return PR_FALSE;
#else
    if (!m_hBitmap)
        return PR_FALSE;
#endif	
    /* 
     * Check to see if this CL_Drawable was the last one to use
     * this drawable. If not, someone else might have modified
     * the bits since the last time this CL_Drawable wrote to
     * to them.
     */
    if ((newState & CL_LOCK_DRAWABLE_FOR_READ) &&
	(m_pOwner != pCLDrawable))
        return PR_FALSE;

    m_pOwner = pCLDrawable;

	//mwh - If we are using directDraw, don't do the palette stuff..
    // If the last user of this drawable relinquished it, then 
    // we need to select the new user's palette into the DC.
#ifdef DDRAW
    if (!(pWinCX->GetBackSurface()) && !m_hSelectedPalette && m_hParentPal) {
#else
    if (!m_hSelectedPalette && m_hParentPal) {
#endif
        ::SelectPalette(m_hDC, m_hParentPal, FALSE);
        m_hSelectedPalette = m_hParentPal;
    }
        
    return PR_TRUE;
}
 
// Set the required dimensions of the drawable. 
void 
COffscreenDrawable::SetDimensions(int32 lWidth, int32 lHeight)
{

    if ((lWidth > m_lWidth) || (lHeight > m_lHeight)) {
	
	/* 
	 * If there is only one client of the backing store,
	 * we can resize it to the dimensions specified.
	 * Otherwise, we can make it larger, but not smaller
	 */
	if (m_uRefCnt > 1) {
	    if (lWidth < m_lWidth)
	        lWidth = m_lWidth;
	    if (lHeight < m_lHeight)
	        lHeight = m_lHeight;
	}

	/* If we already have a bitmap, get rid of it */
	if (m_hBitmap && m_hDC) {
	    ::SelectObject(m_hDC, m_hSaveBitmap);
	    ::DeleteObject(m_hBitmap);
	    m_hBitmap = NULL;
	}
	CWinCX* pWinCX =  (CWinCX*)m_parentContext;

	//mwh - If we are using directDraw, don't create a compatible DC here.
#ifdef DDRAW
	if (!(pWinCX->GetBackSurface())) {
#endif
		if (!m_hDC) {
				m_hDC = ::CreateCompatibleDC(m_hParentDC); 
				if (!m_hDC)
					return;
            
			SetPalette(m_hParentPal);
			::SetBkMode(m_hDC, TRANSPARENT);
		}

		m_hBitmap = ::CreateCompatibleBitmap(m_hParentDC,
											 CASTINT(lWidth), CASTINT(lHeight));
			if (!m_hBitmap) {
				/* 
				 * If the bitmap allocation failed, we just delete all offscreen
				 * resources and fail locking and hence, go fall back to
				 * onscreen drawing.
				 */
				delete_offscreen();
			}
		m_hSaveBitmap = (HBITMAP)::SelectObject(m_hDC, m_hBitmap);
#ifdef DDRAW
	}
#endif
	m_lWidth = lWidth;
	m_lHeight = lHeight;
    }
}

void
COffscreenDrawable::SetPalette(HPALETTE hPal)
{
	CWinCX* pWinCX =  (CWinCX*)m_parentContext;
#ifdef DDRAW
	if (!(pWinCX->GetBackSurface())) {
#endif
		HPALETTE hOldPalette = NULL;
    
		if(hPal && m_hDC)    {
			hOldPalette = ::SelectPalette(m_hDC, hPal, FALSE);
			// don't realize palette with memory DC.
				m_hSelectedPalette = hPal;
		}
		
		if (!m_hSavePalette)
			m_hSavePalette = hOldPalette;
		m_hParentPal = hPal;
#ifdef DDRAW
	}
#endif
}

PRBool
fe_lock_drawable(CL_Drawable *drawable, CL_DrawableState state)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);

    return pDrawable->LockDrawable(drawable, state);
}

void
fe_init_drawable(CL_Drawable *drawable)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
    
    pDrawable->InitDrawable(drawable);
}

void
fe_relinquish_drawable(CL_Drawable *drawable)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
    
    pDrawable->RelinquishDrawable(drawable);
}

void
fe_set_drawable_origin(CL_Drawable *drawable, int32 lOrgX, int32 lOrgY)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
    
    pDrawable->SetOrigin(lOrgX, lOrgY);
}

void
fe_get_drawable_origin(CL_Drawable *drawable, int32 *plOrgX, int32 *plOrgY)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
    
    pDrawable->GetOrigin(plOrgX, plOrgY);
}

void
fe_set_drawable_clip(CL_Drawable *drawable, FE_Region hClipRgn)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);

    pDrawable->SetClip(hClipRgn);
}

void
fe_restore_drawable_clip(CL_Drawable *drawable)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);

    pDrawable->SetClip(NULL);
}

void
fe_copy_pixel(CL_Drawable *pSrc, CL_Drawable *pDst, FE_Region hCopyRgn)
{
    CDrawable *pSrcDrawable = (CDrawable *)CL_GetDrawableClientData(pSrc);
    CDrawable *pDstDrawable = (CDrawable *)CL_GetDrawableClientData(pDst);

    pDstDrawable->CopyPixels(pSrcDrawable, hCopyRgn);
}

void
fe_drawable_set_dimensions(CL_Drawable *drawable, uint32 lWidth, uint32 lHeight)
{
    CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);

    pDrawable->SetDimensions(lWidth, lHeight);
}

CL_DrawableVTable wfe_drawable_vtable = {
    fe_lock_drawable,
    fe_init_drawable,
    fe_relinquish_drawable,
    NULL,
    fe_set_drawable_origin,
    fe_get_drawable_origin,
    fe_set_drawable_clip,
    fe_restore_drawable_clip,
    fe_copy_pixel,
    fe_drawable_set_dimensions
};

#endif // LAYERS
