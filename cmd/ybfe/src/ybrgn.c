/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*   ybrgn.c --- yb functions dealing with front-end */

#include "fe_rgn.h"

FE_Region
FE_CreateRegion()
{
}

FE_Region
FE_CreateRectRegion(XP_Rect *rect)
{
}

void
FE_DestroyRegion(FE_Region region)
{
}

FE_Region
FE_CopyRegion(FE_Region src,
	      FE_Region dst)
{
}

FE_Region 
FE_SetRectRegion(FE_Region region, 
		 XP_Rect *rect)
{
}

void 
FE_IntersectRegion(FE_Region src1, 
		   FE_Region src2, 
		   FE_Region dst)
{
}

void 
FE_UnionRegion(FE_Region src1, 
	       FE_Region src2, 
	       FE_Region dst)
{
}

void 
FE_SubtractRegion(FE_Region src1, 
		  FE_Region src2, 
		  FE_Region dst)
{
}

XP_Bool 
FE_IsEmptyRegion(FE_Region region)
{
}

void 
FE_GetRegionBoundingBox(FE_Region region, 
			XP_Rect *bbox)
{
}

XP_Bool 
FE_IsEqualRegion(FE_Region rgn1, 
		 FE_Region rgn2)
{
}

void 
FE_OffsetRegion(FE_Region region, 
		int32 xOffset, 
		int32 yOffset)
{
}

XP_Bool 
FE_RectInRegion(FE_Region region, 
		XP_Rect *rect)
{
}

/* For each rectangle that makes up this region, call the func */
void 
FE_ForEachRectInRegion(FE_Region region, 
		       FE_RectInRegionFunc func,
		       void * closure)
{
}

#ifdef DEBUG
void 
FE_HighlightRect(void *context, 
		 XP_Rect *rect, 
		 int how_much)
{
}

void 
FE_HighlightRegion(void *context, 
		   FE_Region region, 
		   int how_much)
{
}
#endif /* DEBUG */
