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
/* 
 *  xp_rect.c - Simple rectangle operations
 */


#include "xp.h"
#include "xp_rect.h"

#ifndef MIN
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#endif

void
XP_CopyRect(XP_Rect *src, XP_Rect *dest)
{
    XP_MEMCPY(dest, src, sizeof (XP_Rect));
}

PRBool
XP_EqualRect(XP_Rect *r1, XP_Rect *r2)
{
  return ((r1->left == r2->left) && (r1->top == r2->top) &&
	  (r1->bottom == r2->bottom) && (r1->right == r2->right));
}

/* Offset rectangle in 2D space. This could be a macro */
void
XP_OffsetRect(XP_Rect *rect, int32 x_offset, int32 y_offset)
{
    XP_ASSERT(rect);
    
    rect->left += x_offset;
    rect->top += y_offset;
    rect->right += x_offset;
    rect->bottom += y_offset;
}

/* TRUE if point is within the boundary of the rect */
PRBool
XP_PointInRect(XP_Rect *rect, int32 x, int32 y)
{
    XP_ASSERT(rect);
    
    return (PRBool)((rect->left <= x) &&
            (rect->top <= y) &&
            (rect->right > x) &&
            (rect->bottom > y));
}

/* TRUE if any parts of the two rectangles overlap */
PRBool
XP_RectsOverlap(XP_Rect *rect1, XP_Rect *rect2)
{
    XP_ASSERT(rect1);
    XP_ASSERT(rect2);
    
    return (PRBool)((rect1->right > rect2->left) &&
            (rect1->left < rect2->right) &&
            (rect1->bottom > rect2->top) &&
            (rect1->top < rect2->bottom));
}

/* TRUE if rect2 is entirely contained within rect1 */
PRBool
XP_RectContainsRect(XP_Rect *rect1, XP_Rect *rect2)
{
    XP_ASSERT(rect1);
    XP_ASSERT(rect2);
    
    return (PRBool)((rect1->right >= rect2->right) &&
            (rect1->left <= rect2->left) &&
            (rect1->bottom >= rect2->bottom) &&
            (rect1->top <= rect2->top));
}

/* dst = src1 intersect src2. dst can be one of src1 or src2 */
void
XP_IntersectRect(XP_Rect *src1, XP_Rect *src2, XP_Rect *dst)
{
    XP_ASSERT(src1);
    XP_ASSERT(src2);
    XP_ASSERT(dst);
    
    if (!XP_RectsOverlap(src1, src2)) {
        dst->left = dst->top = dst->right = dst->bottom = 0;
    }
    else {
        dst->left = MAX(src1->left, src2->left);
        dst->top = MAX(src1->top, src2->top);
        dst->right = MIN(src1->right, src2->right);
        dst->bottom = MIN(src1->bottom, src2->bottom);
    }
}

/* dst = bounding box of src1 and src2. dst can be one of src1 or src2 */
void
XP_RectsBbox(XP_Rect *src1, XP_Rect *src2, XP_Rect *dst)
{
    XP_ASSERT(src1);
    XP_ASSERT(src2);
    XP_ASSERT(dst);
    
    dst->left = MIN(src1->left, src2->left);
    dst->top = MIN(src1->top, src2->top);
    dst->right = MAX(src1->right, src2->right);
    dst->bottom = MAX(src1->bottom, src2->bottom);
}

/* Does the rectangle enclose any pixels? */
PRBool
XP_IsEmptyRect(XP_Rect *rect)
{
    XP_ASSERT(rect);
    
    return (PRBool)((rect->left == rect->right) ||
                    (rect->top == rect->bottom));
}
