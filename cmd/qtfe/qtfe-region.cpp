/* $Id: qtfe-region.cpp,v 1.1 1998/09/25 18:01:38 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Haavard Nord.
 * Further developed by Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik
 * Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include "fe_rgn.h"


#include <qregion.h>

QRect qRect( XP_Rect *r )
{
    return QRect( r->left, r->top,
		  r->right - r->left +1,
		  r->bottom - r->top +1 );
}

#define QRGN(r)  ((QRegion*)r)

extern "C" FE_Region
FE_CreateRegion()
{
    return new QRegion();
}

extern "C" FE_Region
FE_CreateRectRegion(XP_Rect *rect)
{
    return new QRegion( qRect(rect) );
}

extern "C" FE_Region
FE_SetRectRegion(FE_Region region, XP_Rect *rect)
{
    ASSERT( region );
    *QRGN(region) = QRegion( qRect(rect) );
    return region;
}

extern "C" void
FE_DestroyRegion(FE_Region region)
{
    ASSERT( region );
    delete QRGN(region);
}

extern "C" FE_Region
FE_CopyRegion(FE_Region src, FE_Region dst)
{
    ASSERT( src );
    if ( dst )
	*QRGN(dst) = *QRGN(src);
    else
	dst = new QRegion( *QRGN(src) );
    return dst;
}

/* dst can be one of src1 or src2 */
extern "C" void
FE_IntersectRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
    ASSERT( src1 && src2 && dst );
    *QRGN(dst) = QRGN(src1)->intersect( *QRGN(src2) );
}

/* dst can be one of src1 or src2 */
extern "C" void
FE_UnionRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
    ASSERT( src1 && src2 && dst );
    *QRGN(dst) = QRGN(src1)->unite( *QRGN(src2) );
}

/* dst can be one of src1 or src2 */
extern "C" void
FE_SubtractRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
    ASSERT( src1 && src2 && dst );
    *QRGN(dst) = QRGN(src1)->subtract( *QRGN(src2) );
}

extern "C" XP_Bool
FE_IsEmptyRegion(FE_Region region)
{
    ASSERT(region);
    return QRGN(region)->isEmpty();
}

extern "C" void
FE_GetRegionBoundingBox(FE_Region region, XP_Rect *bbox)
{
    ASSERT(region);
    QRect r = QRGN(region)->boundingRect();
    bbox->left   = r.left();
    bbox->right  = r.right();
    bbox->top    = r.top();
    bbox->bottom = r.bottom();
}

extern "C" XP_Bool
FE_IsEqualRegion(FE_Region rgn1, FE_Region rgn2)
{
    ASSERT(rgn1 && rgn2);
    return *QRGN(rgn1) == *QRGN(rgn2);
}

extern "C" void
FE_OffsetRegion(FE_Region region, int32 x_offset, int32 y_offset)
{
    ASSERT(region);
    QRGN(region)->translate( x_offset, y_offset );
}

extern "C" XP_Bool
FE_RectInRegion(FE_Region region, XP_Rect *rect)
{
    ASSERT(region);
    return QRGN(region)->contains( qRect(rect) );
}

extern "C" void
FE_ForEachRectInRegion(FE_Region region, FE_RectInRegionFunc func,
                       void *closure)
{
    ASSERT(region && closure);
    XP_Rect rect;
    QArray<QRect> a = QRGN(region)->rects();
    for ( int i=0; i<(int)a.size(); i++ ) {
	QRect r = a[i];
	rect.left   = r.left();
	rect.top    = r.top();
	rect.right  = r.right()+1; // +1 because it's in XRectangle-style
	rect.bottom = r.bottom()+1;
	(*func)(closure,&rect);
    }
}

#ifdef DEBUG

#if 0
static void
fe_InvertRect(MWContext *context, XP_Rect *rect)
{
}
#endif

extern "C" void
FE_HighlightRect(void *c, XP_Rect *rect, int how_much)
{
    //    debug( "FE_HighlightRect not implemented (debug function)" );
}

extern "C" void
FE_HighlightRegion(void *c, FE_Region region, int how_much)
{
    //    debug( "FE_HighlightRegion not implemented (debug function)" );
}

#endif // DEBUG
