/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* Region-related definitions and prototypes */

#ifndef _FE_RGN_H_
#define _FE_RGN_H_

#ifdef LAYERS

#include "xp_core.h"
#include "xp_rect.h"
/******************Definitions and Types************/

/* For Windows only: Should we use the MFC CRgn class for region stuff? */
#ifdef XP_WIN
#undef FE_RGN_USE_MFC
#endif /* XP_WIN */

#ifdef XP_WIN

#ifdef FE_RGN_USE_MFC
#define FE_GetMDRegion(rgn) ((CRgn *)rgn)
#else 
/* 
 * Note that the resultant CRgn * does not have to be 
 * explicitly deleted. It is considered a temporary object
 * by the MFC and is deleted the next time we have idle
 * time in the event loop.
 */
#define FE_GetMDRegion(rgn) ((HRGN)rgn)
#endif /* FE_RGN_USE_MFC */

#elif defined(XP_UNIX)
#define FE_GetMDRegion(rgn) ((Region)rgn)
#elif defined(XP_MAC)
#define FE_GetMDRegion(rgn) ((RgnHandle)rgn)
#else
#define FE_GetMDRegion(rgn) (rgn)
#endif /* XP_WIN */

#ifdef XP_WIN
#define FE_MAX_REGION_COORDINATE 0x7FFFFFFF
#else
#define FE_MAX_REGION_COORDINATE 0x7FFF
#endif

/* Setting the clip region to this effectively unsets the clip */
#define FE_NULL_REGION NULL

#define FE_CLEAR_REGION(region)    \
    do {FE_SubtractRegion((region), (region), (region)); } while (0)

/* Function called by FE_ForEachRectInRegion */
typedef void (*FE_RectInRegionFunc)(void *closure, XP_Rect *rect);

/*******************Prototypes**********************/

XP_BEGIN_PROTOS

extern FE_Region FE_CreateRegion(void);

/* Creates a region from a rectangle. Returns */
/* NULL if region can't be created.           */
extern FE_Region FE_CreateRectRegion(XP_Rect *rect);

/* Destroys region. */
extern void FE_DestroyRegion(FE_Region region);

/* Makes a copy of a region. If dst is NULL, creates a new region */
extern FE_Region FE_CopyRegion(FE_Region src, FE_Region dst);

/* Set an existing region to a rectangle */
extern FE_Region FE_SetRectRegion(FE_Region region, XP_Rect *rect);

/* dst = src1 intersect sr2       */
/* dst can be one of src1 or src2 */
extern void FE_IntersectRegion(FE_Region src1, FE_Region src2, FE_Region dst);

/* dst = src1 union src2          */
/* dst can be one of src1 or src2 */
extern void FE_UnionRegion(FE_Region src1, FE_Region src2, FE_Region dst);

/* dst = src1 - src2              */
/* dst can be one of src1 or src2 */
extern void FE_SubtractRegion(FE_Region src1, FE_Region src2, FE_Region dst);

/* Returns TRUE if the region contains no pixels */
extern XP_Bool FE_IsEmptyRegion(FE_Region region);

/* Returns the bounding rectangle of the region */
extern void FE_GetRegionBoundingBox(FE_Region region, XP_Rect *bbox);

/* TRUE if rgn1 == rgn2 */
extern XP_Bool FE_IsEqualRegion(FE_Region rgn1, FE_Region rgn2);

/* Moves a region by the specified offsets */
extern void FE_OffsetRegion(FE_Region region, int32 xOffset, int32 yOffset);

/* Is any part of the rectangle in the specified region */
extern XP_Bool FE_RectInRegion(FE_Region region, XP_Rect *rect);

/* For each rectangle that makes up this region, call the func */
extern void FE_ForEachRectInRegion(FE_Region region, 
								   FE_RectInRegionFunc func,
								   void * closure);

#ifdef DEBUG
extern void FE_HighlightRect(void *context, XP_Rect *rect, int how_much);
extern void FE_HighlightRegion(void *context, FE_Region region, int how_much);
#endif /* DEBUG */

XP_END_PROTOS

#endif /* LAYERS */

#endif /* _FE_RGN_H_ */
