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

//	macregion.c

//	FE specific region operations

	// Netscape
#include "client.h"
#include "fe_rgn.h"
	// system
#include <Quickdraw.h>

#ifdef LAYERS

FE_Region FE_CreateRegion()
{
	RgnHandle rgnH = NewRgn();
	
	// BUGBUG What's the right thing to do for memory/allocation errors.
	// Assertions are not the right way to go. 
	XP_ASSERT(rgnH != nil);
	
	return (FE_Region)rgnH;
}

/* Creates a region from a rectangle. Returns */
/* NULL if region can't be created.           */
FE_Region 
FE_CreateRectRegion(XP_Rect *rect)
{
	RgnHandle rgnH;
	
	XP_ASSERT(rect != nil);
	
	rgnH = NewRgn();
		
	XP_ASSERT(rgnH != nil);

	SetRectRgn(rgnH, rect->left, rect->top, rect->right, rect->bottom);
	
	return (FE_Region)rgnH;
}

/* Destroys region. */
void 
FE_DestroyRegion(FE_Region region)
{
	DisposeRgn((RgnHandle)region);
}

/* Makes a copy of a region. If dst is NULL, creates a new region */
FE_Region 
FE_CopyRegion(FE_Region src, FE_Region dst)
{
	RgnHandle copyRgnH;
	
	XP_ASSERT(src);
	
	if (dst != nil) 
		copyRgnH = (RgnHandle)dst;
	else {
		copyRgnH = NewRgn();
		XP_ASSERT(copyRgnH != nil);
	}
	
	CopyRgn((RgnHandle)src, copyRgnH);
	
	return (FE_Region)copyRgnH;
}

/* Set an existing region to a rectangle */
FE_Region 
FE_SetRectRegion(FE_Region region, XP_Rect *rect)
{
	XP_ASSERT(region);
	XP_ASSERT(rect);
	
	SetRectRgn((RgnHandle)region, rect->left, rect->top, rect->right, rect->bottom);
	
	return region;
}

/* dst = src1 intersect sr2       */
/* dst can be one of src1 or src2 */
void 
FE_IntersectRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
	XP_ASSERT(src1);
	XP_ASSERT(src2);
	XP_ASSERT(dst);
	
	SectRgn((RgnHandle)src1, (RgnHandle)src2, (RgnHandle)dst);
}

/* dst = src1 union src2          */
/* dst can be one of src1 or src2 */
void FE_UnionRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
	XP_ASSERT(src1);
	XP_ASSERT(src2);
	XP_ASSERT(dst);
	
	UnionRgn((RgnHandle)src1, (RgnHandle)src2, (RgnHandle)dst);
}

/* dst = src1 - src2              */
/* dst can be one of src1 or src2 */
void FE_SubtractRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
	XP_ASSERT(src1);
	XP_ASSERT(src2);
	XP_ASSERT(dst);
	
	DiffRgn((RgnHandle)src1, (RgnHandle)src2, (RgnHandle)dst);
}

/* Returns TRUE if the region contains no pixels */
XP_Bool 
FE_IsEmptyRegion(FE_Region region)
{
	XP_ASSERT(region);
	
	return (XP_Bool)EmptyRgn((RgnHandle)region);
}

/* Returns the bounding rectangle of the region */
void 
FE_GetRegionBoundingBox(FE_Region region, XP_Rect *bbox)
{
	RgnHandle rgnH = (RgnHandle)region;
	
	XP_ASSERT(region);
	XP_ASSERT(bbox);

	bbox->left = (**rgnH).rgnBBox.left;
	bbox->top = (**rgnH).rgnBBox.top;
	bbox->right = (**rgnH).rgnBBox.right;
	bbox->bottom = (**rgnH).rgnBBox.bottom;
}

/* TRUE if rgn1 == rgn2 */
XP_Bool
FE_IsEqualRegion(FE_Region rgn1, FE_Region rgn2)
{
	XP_ASSERT(rgn1);
	XP_ASSERT(rgn2);
	
	return (XP_Bool)EqualRgn((RgnHandle)rgn1, (RgnHandle)rgn2);
}

/* Moves a region by the specified offsets */
void 
FE_OffsetRegion(FE_Region region, int32 xOffset, int32 yOffset)
{
	XP_ASSERT(region);
	
	OffsetRgn((RgnHandle)region, xOffset, yOffset);
}

/* Is any part of the rectangle in the specified region */
XP_Bool
FE_RectInRegion(FE_Region region, XP_Rect *rect)
{
	Rect macRect;
	
	XP_ASSERT(region);
	XP_ASSERT(rect);
	
	macRect.left = rect->left;
	macRect.top = rect->top;
	macRect.right = rect->right;
	macRect.bottom = rect->bottom;
	
	return (XP_Bool)RectInRgn(&macRect, (RgnHandle)region);
}

/* For each rectangle that makes up this region, call the func */
/* This is a minor adaptation of code written by Hugh Fisher
   and published in the RegionToRectangles example in the InfoMac archives.
*/
void
FE_ForEachRectInRegion(FE_Region rgn, FE_RectInRegionFunc func, void *closure)
{
#define EndMark 	32767
#define MaxY		32767
#define StackMax	1024

	typedef struct {
		short	size;
		Rect	bbox;
		short	data[];
		} ** Internal;
	
	Internal region;
	short	 width, xAdjust, y, index, x1, x2, x;
	XP_Rect	 box;
	short	 stackStorage[1024];
	short *	 buffer;
	RgnHandle r = (RgnHandle)rgn;
	
	region = (Internal)r;
	
	/* Check for plain rectangle */
	if ((**region).size == 10) {
		box.left = (**region).bbox.left;
		box.top = (**region).bbox.top;
		box.right = (**region).bbox.right;
		box.bottom = (**region).bbox.bottom;
		func(closure, &box);
		return;
	}
	/* Got to scale x coordinates into range 0..something */
	xAdjust = (**region).bbox.left;
	width = (**region).bbox.right - xAdjust;
	/* Most regions will be less than 1024 pixels wide */
	if (width < StackMax)
		buffer = stackStorage;
	else {
		buffer = (short *)NewPtr(width * 2);
		if (buffer == NULL)
			/* Truly humungous region or very low on memory.
			   Quietly doing nothing seems to be the
			   traditional Quickdraw response. */
			return;
	}
	/* Initialise scan line list to bottom edges */
	for (x = (**region).bbox.left; x < (**region).bbox.right; x++)
		buffer[x - xAdjust] = MaxY;
	index = 0;
	/* Loop until we hit an empty scan line */
	while ((**region).data[index] != EndMark) {
		y = (**region).data[index];
		index ++;
		/* Loop through horizontal runs on this line */
		while ((**region).data[index] != EndMark) {
			x1 = (**region).data[index];
			index ++;
			x2 = (**region).data[index];
			index ++;
			x = x1;
			while (x < x2) {
				if (buffer[x - xAdjust] < y) {
					/* We have a bottom edge - how long for? */
					box.left = x;
					box.top  = buffer[x - xAdjust];
					while (x < x2 && buffer[x - xAdjust] == box.top) {
						buffer[x - xAdjust] = MaxY;
						x ++;
					}
					/* Pass to client proc */
					box.right  = x;
					box.bottom = y;
					func(closure, &box);
				} else {
					/* This becomes a top edge */
					buffer[x - xAdjust] = y;
					x ++;
				}
			}
		}
		index ++;
	}
	/* Clean up after ourselves */
	if (width >= StackMax)
		DisposePtr((Ptr)buffer);
#undef EndMark
#undef MaxY
#undef StackMax
}
#endif
