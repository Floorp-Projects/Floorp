/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 *   region.cpp --- FE specific region operations
 */



#include "stdafx.h"

//mwh don't use MFC on Nav 4.0
//#define RGN_USE_MFC

/* Creates an empty region. Returns NULL if region can't be created */
FE_Region 
FE_CreateRegion()
{
#ifdef FE_RGN_USE_MFC
	CRgn *pRgn = new CRgn;

	pRgn->CreateRectRgn(0, 0, 0, 0);

	return (FE_Region)pRgn;
#else
	return (FE_Region)CreateRectRgn(0, 0, 0, 0);
#endif
}

/* Creates a region from a rectangle. Returns NULL if region can't be created */
FE_Region
FE_CreateRectRegion(XP_Rect *rect)
{
#ifdef FE_RGN_USE_MFC
	CRgn *pRgn = new CRgn;
	pRgn->CreateRectRgn(rect->left, rect->top, rect->right, rect->bottom);
	return (FE_Region)pRgn;
#else
//	return (FE_Region)CreateRectRgn(rect->left, rect->top, 
//									rect->right, rect->bottom);
	HRGN rgn = CreateRectRgn(CASTINT(rect->left), CASTINT(rect->top), 
									CASTINT(rect->right), CASTINT(rect->bottom));
	if (rgn) return (FE_Region)rgn;
	else
		return (FE_Region)CreateRectRgn(0, 0, 0, 0);
#endif
}

/* Set an existing region to a rectangle */
FE_Region 
FE_SetRectRegion(FE_Region region, XP_Rect *rect)
{
	XP_ASSERT(region);
#ifdef FE_RGN_USE_MFC
	((CRgn *)region)->SetRectRgn(rect->left, rect->top,
							   rect->right, rect->bottom);
	return region;
#else
	::SetRectRgn((HRGN)region, CASTINT(rect->left), CASTINT(rect->top),
							   CASTINT(rect->right), CASTINT(rect->bottom));
	return region;
#endif
}

/* Destroys region */
void
FE_DestroyRegion(FE_Region region)
{
#ifndef FE_RGN_USE_MFC
	BOOL result;
#endif
	
	XP_ASSERT(region);
	
#ifdef FE_RGN_USE_MFC
	((CRgn *)region)->DeleteObject();
	delete ((CRgn *)region);
#else
	result = DeleteObject((HRGN)region);

	XP_ASSERT(result);
#endif
}

/* Make a copy of a region */
FE_Region
FE_CopyRegion(FE_Region src, FE_Region dst)
{

#ifdef FE_RGN_USE_MFC
	CRgn * pCopyRgn;

	XP_ASSERT(src);

	if (dst != NULL) 
	    pCopyRgn = (CRgn *)dst;
	else {
		pCopyRgn = new CRgn;
		pCopyRgn->CreateRectRgn(0, 0, 0, 0);
	}

	pCopyRgn->CopyRgn((CRgn *)src);
	
	return pCopyRgn;
#else
	HRGN copyRegion;
  
	XP_ASSERT(src);
  
	if (dst != NULL)
	  copyRegion = (HRGN)dst;
	else {
		/* Create an empty region */
		copyRegion = CreateRectRgn(0, 0, 0, 0);
		
		if (copyRegion == NULL)
		  return NULL;
	}
	
	/* Copy the region */
	if (CombineRgn(copyRegion, 
				   (HRGN)src, 
				   (HRGN)src, 
				   RGN_COPY) == ERROR) {
		DeleteObject(copyRegion);
		return NULL;
	}
  
	return (FE_Region)copyRegion;
#endif
}

/* dst = src1 intersect sr2       */
/* dst can be one of src1 or src2 */
void
FE_IntersectRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
	XP_ASSERT(src1);
	XP_ASSERT(src2);
	XP_ASSERT(dst);

#ifdef FE_RGN_USE_MFC
	((CRgn *)dst)->CombineRgn((CRgn *)src1, (CRgn *)src2, RGN_AND);
#else
	CombineRgn((HRGN)dst, (HRGN)src1, (HRGN)src2, RGN_AND);
#endif
}

/* dst = src1 union src2          */
/* dst can be one of src1 or src2 */
void
FE_UnionRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
	XP_ASSERT(src1);
	XP_ASSERT(src2);
	XP_ASSERT(dst);

#ifdef FE_RGN_USE_MFC
	((CRgn *)dst)->CombineRgn((CRgn *)src1, (CRgn *)src2, RGN_OR);
#else
	CombineRgn((HRGN)dst, (HRGN)src1, (HRGN)src2, RGN_OR);
#endif
}

/* dst = src1 - src2              */
/* dst can be one of src1 or src2 */
void
FE_SubtractRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
	XP_ASSERT(src1);
	XP_ASSERT(src2);
	XP_ASSERT(dst);

#ifdef FE_RGN_USE_MFC
	((CRgn *)dst)->CombineRgn((CRgn *)src1, (CRgn *)src2, RGN_DIFF);
#else
	CombineRgn((HRGN)dst, (HRGN)src1, (HRGN)src2, RGN_DIFF);
#endif
}

/* Returns TRUE if the region contains no pixels */
XP_Bool
FE_IsEmptyRegion(FE_Region region)
{
	int result;
	
	XP_ASSERT(region);
	
#ifdef FE_RGN_USE_MFC
	result = ((CRgn *)region)->OffsetRgn(0, 0);
#else
	/* This might not be the best way to find out, but it's one of them */
	result = OffsetRgn((HRGN)region, 0, 0);
#endif
	
	if (result == NULLREGION)
	  return TRUE;
	else
	  return FALSE;
}

/* Returns the bounding rectangle of the region */
void
FE_GetRegionBoundingBox(FE_Region region, XP_Rect *bbox)
{
	RECT rect;

	XP_ASSERT(region);
	XP_ASSERT(bbox);
	
#ifdef FE_RGN_USE_MFC
	((CRgn *)region)->GetRgnBox((LPRECT)&rect);
#else
	GetRgnBox((HRGN)region, (LPRECT)&rect);
#endif

	bbox->left = rect.left;
	bbox->top = rect.top;
	bbox->right = rect.right;
	bbox->bottom = rect.bottom;
}

/* TRUE if rgn1 == rgn2 */
XP_Bool
FE_IsEqualRegion(FE_Region rgn1, FE_Region rgn2)
{
	XP_ASSERT(rgn1);
	XP_ASSERT(rgn2);
	
#ifdef FE_RGN_USE_MFC
	return ((CRgn *)rgn1)->EqualRgn((CRgn *)rgn2);
#else
	return (EqualRgn((HRGN)rgn1, (HRGN)rgn2));
#endif
}

/* Moves a region by the specified offsets */
void
FE_OffsetRegion(FE_Region region, int32 x_offset, int32 y_offset)
{
	int result;
	
#ifdef FE_RGN_USE_MFC
	result = ((CRgn *)region)->OffsetRgn(x_offset, y_offset);
#else
	result = OffsetRgn((HRGN)region, CASTINT(x_offset), CASTINT(y_offset));
#endif

	XP_ASSERT(result != ERROR);
}

/* Returns TRUE if any part of the rectangle is in the specified region */
XP_Bool
FE_RectInRegion(FE_Region region, XP_Rect *rect)
{
   RECT box;
 
   XP_ASSERT(region);
   XP_ASSERT(rect);
 
   box.left = CASTINT(rect->left);
   box.top = CASTINT(rect->top);
   box.right = CASTINT(rect->right);
   box.bottom = CASTINT(rect->bottom);

#ifdef FE_RGN_USE_MFC
   return ((CRgn *)region)->RectInRegion(&box);
#else
   return RectInRegion((HRGN)region, &box);
#endif
}

/* Calls the specified function for each rectangle that makes up the region */
void
FE_ForEachRectInRegion(FE_Region region, FE_RectInRegionFunc func, void *closure)
{
#ifndef _WIN32
	/* 
	 * For 16-bit Windows, we can't get at the rectangles that make up a region,
	 * so we just call the function with the bounding box of the entire region.
	 */
	RECT rect;
    XP_Rect xprect;

#ifdef FE_RGN_USE_MFC
	((CRgn *)region)->GetRgnBox((LPRECT)&rect);
#else
	GetRgnBox((HRGN)region, (LPRECT)&rect);
#endif

	xprect.left = rect.left;
	xprect.top = rect.top;
	xprect.right = rect.right;
	xprect.bottom = rect.bottom;
	(*func)(closure, &xprect);
	
#else
	LPRGNDATA pRgnData;
	LPRECT pRects;
	DWORD dwCount, dwResult;
	unsigned int num_rects;
	XP_Rect rect;
#ifdef FE_RGN_USE_MFC
	CRgn *pRgn = (CRgn *)region;
#endif

	XP_ASSERT(region);
	XP_ASSERT(func);

	/* Get the size of the region data */
#ifdef FE_RGN_USE_MFC
	dwCount = pRgn->GetRegionData(NULL, 0);
#else
	dwCount = GetRegionData((HRGN)region, 0, NULL);
#endif

	XP_ASSERT(dwCount != 0);
	if (dwCount == 0)
	  return;

	pRgnData = (LPRGNDATA)XP_ALLOC(dwCount);

	XP_ASSERT(pRgnData != NULL);
	if (pRgnData == NULL)
	  return;

#ifdef FE_RGN_USE_MFC
	dwResult = pRgn->GetRegionData(pRgnData, dwCount);
#else
	dwResult = GetRegionData((HRGN)region, dwCount, pRgnData);
#endif

	XP_ASSERT(dwResult != 0);
	if (dwResult == 0) {
		XP_FREE(pRgnData);
		return;
	}
  
	for (pRects = (LPRECT)pRgnData->Buffer, num_rects = 0; 
		 num_rects < pRgnData->rdh.nCount; 
		 num_rects++, pRects++) {
		rect.left = pRects->left;
		rect.top = pRects->top;
		rect.right = pRects->right;
		rect.bottom = pRects->bottom;
		(*func)(closure, &rect);
	}

	XP_FREE(pRgnData);
#endif /* XP_WIN32 */
}

void
FE_HighlightRegion(void *pC, FE_Region region, int how_much)
{
#if defined(DEBUG) && defined(_WIN32)
    MWContext *pContext = (MWContext *)pC;
    HDC	hdc = ::GetDC(PANECX(pContext)->GetPane());

    InvertRgn(hdc, (HRGN)region);
    Sleep(how_much * 2 / 3);
    InvertRgn(hdc, (HRGN)region);
    Sleep(how_much / 3);
	::ReleaseDC(PANECX(pContext)->GetPane(), hdc);
#else
	XP_ASSERT(0);				/* Not supported on Win16 */
#endif
}

