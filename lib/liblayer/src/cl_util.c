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
 *  cl_paint.c - The default layer creation routine
 */


#include "prtypes.h"
#include "layers.h"
#include "cl_priv.h"

/* Convert rectangle from window to document coordinate system */
void
CL_WindowToDocumentRect(CL_Compositor *compositor, XP_Rect *rect)
{
    XP_OffsetRect(rect, compositor->x_offset, compositor->y_offset);
}

/* Convert rectangle from document to window coordinate system */
void
CL_DocumentToWindowRect(CL_Compositor *compositor, XP_Rect *rect)
{
    XP_OffsetRect(rect, -compositor->x_offset, -compositor->y_offset);
}

/* Convert a rect from window to layer coordinate system */
void
CL_WindowToLayerRect(CL_Compositor *compositor, CL_Layer *layer,
                     XP_Rect *rect)
{
    int32 x_offset = compositor->x_offset - layer->x_origin;
    int32 y_offset = compositor->y_offset - layer->y_origin;

    XP_OffsetRect(rect, x_offset, y_offset);
}

/* Convert a rect from window to layer coordinate system */
void
CL_LayerToWindowRect(CL_Compositor *compositor, CL_Layer *layer,
                     XP_Rect *rect)
{
    int32 x_offset = compositor->x_offset - layer->x_origin;
    int32 y_offset = compositor->y_offset - layer->y_origin;

    XP_OffsetRect(rect, -x_offset, -y_offset);
}

/* 
 * The assumption for the next two routines is that the layer is
 * visible within the window i.e. translating to and from layer
 * coordinates won't result in a 16-bit overflow.
 */
/* Convert a region from layer to window coordinate system */
void
CL_LayerToWindowRegion(CL_Compositor *compositor, CL_Layer *layer,
                       FE_Region region)
{
    int32 x_offset = layer->x_origin - compositor->x_offset;
    int32 y_offset = layer->y_origin - compositor->y_offset;

    if ((x_offset <= FE_MAX_REGION_COORDINATE) &&
        (y_offset <= FE_MAX_REGION_COORDINATE))
        FE_OffsetRegion(region, x_offset, y_offset);
}

/* Convert a region from window to layer coordinate system */
void
CL_WindowToLayerRegion(CL_Compositor *compositor, CL_Layer *layer,
                       FE_Region region)
{
    int32 x_offset = compositor->x_offset - layer->x_origin;
    int32 y_offset = compositor->y_offset - layer->y_origin;

    if ((x_offset <= FE_MAX_REGION_COORDINATE) &&
        (y_offset <= FE_MAX_REGION_COORDINATE))
        FE_OffsetRegion(region, x_offset, y_offset);
}

typedef struct {
    int num_rects;
    uint32 covered_area;
} cl_entropy_closure;

static void
cl_region_entropy_rect_func(cl_entropy_closure *closure,
			       XP_Rect *rect)
{
    uint32 rect_area;

    rect_area = (rect->right - rect->left) * (rect->bottom - rect->top); 

    /* Small areas cause higher entropy, since they tend to do 
       lots of clipping when overdraw occurs. */
    if (rect_area > 1000)
        closure->covered_area += rect_area;
    closure->num_rects++;
} 

/* Return a number between 0 and 1.0 indicating the "entropy" of a
   region.  (For now, returns something approximating the fraction of
   uncovered area in the region's bounding box.)  */
float
CL_RegionEntropy(FE_Region region)
{
    uint32 bbox_area;
    XP_Rect bbox;
    cl_entropy_closure closure;


    FE_GetRegionBoundingBox(region, &bbox);
    bbox_area = (bbox.right - bbox.left) * (bbox.bottom - bbox.top);

    closure.num_rects = 0;
    closure.covered_area = 0;
    FE_ForEachRectInRegion(region, 
                           (FE_RectInRegionFunc)cl_region_entropy_rect_func,
                           (void *)&closure);

    if (closure.num_rects <= 1)
	return 0.0F;

    return 1.0F - (float)closure.covered_area / (float)bbox_area;
}


/* Like FE_ForEachRectInRegion(), except that there is no guarantee
   that the individual rects delivered to the callback function
   *exactly* cover the region.  (They may cover more than the region).
   This may be used for more efficient painting because it is faster
   to paint fewer large rects than many small rects.) */
void
CL_ForEachRectCoveringRegion(FE_Region region, FE_RectInRegionFunc func,
			     void *closure)
{
#   define ENTROPY_THRESHOLD 0.85

    float entropy = CL_RegionEntropy(region);
    
    if (entropy < ENTROPY_THRESHOLD) {
	XP_Rect bbox;
	FE_GetRegionBoundingBox(region, &bbox);
	(*func)(closure, &bbox);
    } else {
	FE_ForEachRectInRegion(region, func, closure);
    }
}

void
cl_XorRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
    FE_Region tmp = FE_CreateRegion();
    
    if (tmp == FE_NULL_REGION)  /* OOM */
        return;

    FE_SubtractRegion(src1, src2, tmp);
    FE_SubtractRegion(src2, src1, dst);
    FE_UnionRegion(tmp, dst, dst);
    FE_DestroyRegion(tmp);
}

void 
cl_sanitize_bbox(XP_Rect *bbox)
{
    if (bbox->left > bbox->right)
        bbox->right = bbox->left;
    
    if (bbox->top > bbox->bottom)
        bbox->bottom = bbox->top;
}
