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

/*   gnomergn.c --- gnome functions dealing with front-end */

#include "fe_rgn.h"
#include "xpassert.h"

#include "gnome.h"
#include <gdk/gdkprivate.h>
#include <gdk/gdktypes.h>
#include <X11/X.h>
#include "gnomefe.h"

/* oh, how we love hacking around in x internals */

#include "x_region.h"

/* typedef to make working with GdkRegion** a bit easier */
typedef GdkRegion **GdkRegionHandle;

/* Creates an empty region. Returns NULL if region can't be created */
FE_Region
FE_CreateRegion()
{
  GdkRegionHandle hdl;
  GdkRegion *new_region = gdk_region_new();
  
  if (new_region == NULL) 
    return NULL;

  hdl = (GdkRegionHandle)(malloc(sizeof(GdkRegionHandle)));
  (GdkRegion*)(*hdl) = new_region;
  /* printf("empty Region created at %p\n", hdl); */
  return (FE_Region)hdl;
}

/* Creates a region from a rectangle. Returns NULL if region can't be created */
FE_Region
FE_CreateRectRegion(XP_Rect *rect)
{
  GdkPoint points[4];
  GdkRegion *new_region;
  GdkRegionHandle hdl;

  points[0].x = points[3].x = (short)rect->left;  /* points[] corresponds to */
  points[0].y = points[1].y = (short)rect->top;   /* top-left, top-right,    */
  points[1].x = points[2].x = (short)rect->right; /* bot-left and bot-right  */
  points[3].y = points[2].y = (short)rect->bottom;/* respectively.           */
  
  new_region = gdk_region_polygon(points, 4, GDK_EVEN_ODD_RULE);
  if (new_region == NULL) 
    return NULL;
  hdl = (GdkRegionHandle)(malloc(sizeof(GdkRegionHandle)));
  *hdl=new_region;
  /* printf("Region created from rectangle at %p\n", hdl); */
  /* printf("dimension are %d %d %d %d\n", rect->left, rect->top, 
     rect->right, rect->bottom); */
  return (FE_Region)hdl;
                                /* Fill rule is irrelevant since there */
                                /* should be no self overlap.          */
}

/* Set an existing region to a rectangle.                                  */
/* The purpose of this routine is to avoid the overhead of destroying and  */
/* recreating a region on the Windows platform.  Unfortunately, it doesn't */
/* do much for X.                                                          */
FE_Region 
FE_SetRectRegion(FE_Region region, XP_Rect *rect)
{
  GdkRectangle rectangle;
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  GdkRegionHandle hdl;
  
  XP_ASSERT(reg);
  
  if ( !gdk_region_empty(reg) ) 
    reg = gdk_regions_subtract(reg, reg);
  
  XP_ASSERT(gdk_empty_region(reg));
  
  rectangle.x = (short)rect->left;
  rectangle.y = (short)rect->top;
  rectangle.width = (unsigned short)(rect->right - rect->left);
  rectangle.height = (unsigned short)(rect->bottom - rect->top);

  reg = gdk_region_union_with_rect(reg, &rectangle);
  hdl = (GdkRegionHandle)(malloc(sizeof(GdkRegionHandle)));
  XP_ASSERT(hdl);
  *hdl = reg;
  /* printf("setrectregion done for %p\n", hdl); */
  
  return (FE_Region)hdl;
}

/* Destroys region */
void
FE_DestroyRegion(FE_Region region)
{  
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  XP_ASSERT(reg);
  
  gdk_region_destroy(reg);
  free(region);
  /* printf("region freed at %p\n", region); */
}

/* Make a copy of a region */
FE_Region
FE_CopyRegion(FE_Region src, FE_Region dst)
{

  GdkRegion *srcRegion = (GdkRegion*)(*(GdkRegionHandle)src);
  GdkRegion *dstRegion = (GdkRegion*)(*(GdkRegionHandle)dst);
  GdkRegionHandle hdl;
  GdkRegionHandle cpy;
  
  XP_ASSERT(src);
  XP_ASSERT(srcRegion);
  
  if (dst != NULL) 
    {
      cpy = dst;
    }
  else 
    {
      /* Create an empty region and assign it to the cpy handle*/
      GdkRegion *copyRegion = gdk_region_new();
      cpy = (GdkRegionHandle)malloc( sizeof(GdkRegionHandle ));
      if (copyRegion == NULL || cpy == NULL)
        return NULL;
      *cpy = copyRegion;
    }
  
  /* Copy the region */
  (GdkRegion*)(*cpy) = gdk_regions_union(srcRegion, srcRegion);
  /* wierd: union between two same regions? */
  /*  XUnionRegion((Region)src, (Region)src, (Region)copyRegion); */
  
  /* printf("FE_CopyRegion called and done\n"); */
  return cpy;
}

/* dst = src1 intersect sr2       */
/* dst can be one of src1 or src2 */
void
FE_IntersectRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
  GdkRegion *src1Region = (GdkRegion*)(*(GdkRegionHandle)src1);
  GdkRegion *src2Region = (GdkRegion*)(*(GdkRegionHandle)src2);
  GdkRegion *dstRegion  = (GdkRegion*)(*(GdkRegionHandle)dst);
  GdkRegion *newRegion;
  
  XP_ASSERT(src1Region);
  XP_ASSERT(src2Region);
  XP_ASSERT(dstRegion);
  
  newRegion = gdk_regions_intersect(src1Region, src2Region);
  if (dstRegion == src1Region) 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)src1 =
        (GdkRegion*)*(GdkRegionHandle)dst = 
        newRegion;
    }
  else if (dstRegion == src2Region) 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)src2 = 
        (GdkRegion*)*(GdkRegionHandle)dst = 
        newRegion;
    }
  else 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)dst = newRegion;
    }
  /* printf("done region intersect\n"); */
  /*  XIntersectRegion((Region)src1, (Region)src2, (Region)dst); */
}

/* dst = src1 union src2          */
/* dst can be one of src1 or src2 */
void
FE_UnionRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
  GdkRegion *src1Region = (GdkRegion*)(*(GdkRegionHandle)src1);
  GdkRegion *src2Region = (GdkRegion*)(*(GdkRegionHandle)src2);
  GdkRegion *dstRegion  = (GdkRegion*)(*(GdkRegionHandle)dst);
  GdkRegion *newRegion;

  XP_ASSERT(src1Region);
  XP_ASSERT(src2Region);
  XP_ASSERT(dstRegion);

  newRegion = gdk_regions_union(src1Region, src2Region);
  /*  XUnionRegion((Region)src1, (Region)src2, (Region)dst); */
  if (dstRegion == src1Region) 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)src1 = 
        (GdkRegion*)*(GdkRegionHandle)dst = 
        newRegion;
    }
  else if (dstRegion == src2Region) 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)src2 = 
        (GdkRegion*)*(GdkRegionHandle)dst = 
        newRegion;
    }
  else 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)dst = newRegion;
    } 
  /* printf("done union regions\n"); */
}

/* dst = src1 - src2              */
/* dst can be one of src1 or src2 */
void
FE_SubtractRegion(FE_Region src1, FE_Region src2, FE_Region dst)
{
  GdkRegion *src1Region = (GdkRegion*)(*(GdkRegionHandle)src1);
  GdkRegion *src2Region = (GdkRegion*)(*(GdkRegionHandle)src2);
  GdkRegion *dstRegion  = (GdkRegion*)(*(GdkRegionHandle)dst);
  GdkRegion *newRegion;

  XP_ASSERT(src1Region);
  XP_ASSERT(src2Region);
  XP_ASSERT(dstRegion);
  
  newRegion = gdk_regions_subtract(src1Region, src2Region);
  /* XSubtractRegion((Region)src1, (Region)src2, (Region)dst); */
  if (dstRegion == src1Region) 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)src1 = 
        (GdkRegion*)*(GdkRegionHandle)dst = 
        newRegion;
    }
  else if (dstRegion == src2Region) 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)src2 = 
        (GdkRegion*)*(GdkRegionHandle)dst = 
        newRegion;
    }
  else 
    {
      gdk_region_destroy(dstRegion);
      (GdkRegion*)*(GdkRegionHandle)dst = newRegion;
    }
  /* printf("done subtract region\n"); */
}

/* Returns TRUE if the region contains no pixels */
XP_Bool
FE_IsEmptyRegion(FE_Region region)
{
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  XP_ASSERT(reg);
  
  return (XP_Bool)gdk_region_empty(reg);
}

/* Returns the bounding rectangle of the region */
void
FE_GetRegionBoundingBox(FE_Region region, XP_Rect *bbox)
{
  GdkRectangle rect;
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  
  XP_ASSERT(reg);
  XP_ASSERT(bbox);
  
  gdk_region_get_clipbox(reg, &rect);
  
  bbox->left = (int32)rect.x;
  bbox->top = (int32)rect.y;
  bbox->right = (int32)(rect.x + rect.width);
  bbox->bottom = (int32)(rect.y + rect.height);
  
  /* printf("done boundingbox at %p\n", region);  */
}

/* TRUE if rgn1 == rgn2 */
XP_Bool
FE_IsEqualRegion(FE_Region rgn1, FE_Region rgn2)
{
  GdkRegion *reg1 = (GdkRegion*)(*(GdkRegionHandle)rgn1);
  GdkRegion *reg2 = (GdkRegion*)(*(GdkRegionHandle)rgn2);
  XP_ASSERT(reg1);
  XP_ASSERT(reg2);

  /* printf ("done isequal\n"); */
  return (XP_Bool)gdk_region_equal(reg1,reg2);
}

/* Moves a region by the specified offsets */
void
FE_OffsetRegion(FE_Region region, int32 x_offset, int32 y_offset)
{
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  gdk_region_offset(reg, x_offset, y_offset);
}

/* Returns TRUE if any part of the rectangle is in the specified region */
XP_Bool
FE_RectInRegion(FE_Region region, XP_Rect *rect)
{
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  GdkOverlapType result;
  GdkRectangle box;
  
  XP_ASSERT(reg);
  XP_ASSERT(rect);
  
  box.x = (short)rect->left;
  box.y = (short)rect->top;
  box.width = (unsigned short)(rect->right - rect->left);
  box.height = (unsigned short)(rect->bottom - rect->top);
  
  result = gdk_region_rect_in(reg, &box);
  
  /* printf ("done rectinregion\n"); */
  if (result == GDK_OVERLAP_RECTANGLE_OUT)
    return FALSE;
  else                /* result == RectangleIn || result == RectanglePart */
    return TRUE;
}


/* Calls the specified function for each rectangle that makes up the region */
void
FE_ForEachRectInRegion(FE_Region region, FE_RectInRegionFunc func,
                       void *closure)
{
  GdkRegion *reg = (GdkRegion*)(*(GdkRegionHandle)region);
  Region pRegion;
  register int nbox;
  register BOX *pbox;
  XP_Rect rect;

  XP_ASSERT(reg);
  XP_ASSERT(func);
  
  pRegion = (Region)((GdkRegionPrivate *)reg)->xregion;
  pbox = pRegion->rects;
  nbox = pRegion->numRects;
  
  while(nbox--)
    {
      rect.left = pbox->x1;
      rect.right = pbox->x2;
      rect.top = pbox->y1;
      rect.bottom = pbox->y2;
      (*func)(closure, &rect);
      pbox++;
    }
  /* printf("done foreachrectinregion\n"); */
}

#ifdef DEBUG

void
FE_HighlightRect(void *c, XP_Rect *rect, int how_much)
{
  printf("FE_HighlightRect (empty)\n");
}

void
FE_HighlightRegion(void *c, FE_Region region, int how_much)
{
  printf("FE_HighlightRegion (empty)\n");
}
    
#endif /* DEBUG */






