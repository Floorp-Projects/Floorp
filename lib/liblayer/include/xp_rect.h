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


#ifndef _XP_RECT_H_
#define _XP_RECT_H_

#include "prtypes.h"
#include "cl_types.h"

/* A cross-platform rectangle */
struct _XP_Rect {
  int32 top;
  int32 left;
  int32 bottom;
  int32 right;
};

XP_BEGIN_PROTOS

/* This could be a macro */
extern void XP_CopyRect(XP_Rect *src, XP_Rect *dest);

/* Predicate to compare two rectangles */
extern PRBool XP_EqualRect(XP_Rect *r1, XP_Rect *r2);

/* Offset rectangle in 2D space. This could be a macro */
extern void XP_OffsetRect(XP_Rect *rect, int32 x_offset, int32 y_offset);

/* TRUE if point is within the boundary of the rect */
extern PRBool XP_PointInRect(XP_Rect *rect, int32 x, int32 y);

/* TRUE if any parts of the two rectangles overlap */
extern PRBool XP_RectsOverlap(XP_Rect *rect1, XP_Rect *rect2);

/* TRUE if rect2 is entirely contained within rect1 */
extern PRBool XP_RectContainsRect(XP_Rect *rect1, XP_Rect *rect2);

/* dst = src1 intersect src2. dst can be one of src1 or src2 */
extern void XP_IntersectRect(XP_Rect *src1, XP_Rect *src2, XP_Rect *dst);

/* dst = bounding box of src1 and src2. dst can be one of src1 or src2 */
extern void XP_RectsBbox(XP_Rect *src1, XP_Rect *src2, XP_Rect *dst);

/* Does the rectangle enclose any pixels? */
extern PRBool XP_IsEmptyRect(XP_Rect *rect);

XP_END_PROTOS

#endif /* _XP_RECT_H_ */
