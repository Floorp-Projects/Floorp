/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Stuart Parmenter.
 * Portions created by Stuart Parmenter are Copyright (C) 1998-2000
 * Stuart Parmenter.  All Rights Reserved.  
 *
 * Contributor(s):
 *    Stuart Parmenter <pavlov@netscape.com>
 */

#include <gdk/gdkregion.h>
#include "nsRegionGTK.h"
#include "nsMemory.h"

#ifdef DEBUG_REGIONS
static int nRegions;
#endif

nsRegionGTK::nsRegionGTK()
{
  NS_INIT_ISUPPORTS();

#ifdef DEBUG_REGIONS
  ++nRegions;
  printf("REGIONS+ = %i\n", nRegions);
#endif

  mRegion = nsnull;
}

nsRegionGTK::~nsRegionGTK()
{
#ifdef DEBUG_REGIONS
  --nRegions;
  printf("REGIONS- = %i\n", nRegions);
#endif

  if (mRegion)
    gdk_region_destroy(mRegion);
  mRegion = nsnull;
}

NS_IMPL_ISUPPORTS1(nsRegionGTK, nsIRegion)

nsresult nsRegionGTK::Init(void)
{
  if (mRegion) {
    gdk_region_destroy(mRegion);
    mRegion = nsnull;
  }

  return NS_OK;
}

void nsRegionGTK::SetTo(const nsIRegion &aRegion)
{
  Init();

  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;

  mRegion = gdk_region_copy(pRegion->mRegion);
}

void nsRegionGTK::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Init();
  
  GdkRectangle rect;
  rect.x = aX;
  rect.y = aY;
  rect.width = aWidth;
  rect.height = aHeight;

  mRegion = gdk_region_rectangle(&rect);
}

void nsRegionGTK::Intersect(const nsIRegion &aRegion)
{
  if(!mRegion) {
    NS_WARNING("mRegion is NULL");
    return;
  }

  nsRegionGTK * pRegion = (nsRegionGTK *)&aRegion;

  gdk_region_intersect(mRegion, pRegion->mRegion);
}

void nsRegionGTK::Intersect(PRInt32 aX, PRInt32 aY,
                            PRInt32 aWidth, PRInt32 aHeight)
{
  if(!mRegion) {
    NS_WARNING("mRegion is NULL");
    return;
  }

  GdkRectangle rect;
  rect.x = aX;
  rect.y = aY;
  rect.width = aWidth;
  rect.height = aHeight;

  GdkRegion *tRegion = gdk_region_rectangle(&rect);

  gdk_region_intersect(mRegion, tRegion);
  gdk_region_destroy(tRegion);
}

void nsRegionGTK::Union(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;

  if (pRegion->mRegion && !gdk_region_empty(pRegion->mRegion)) {
    if (mRegion) {
      if (gdk_region_empty(mRegion)) {
        gdk_region_destroy(mRegion);
        mRegion = gdk_region_copy(pRegion->mRegion);
      } else {
        gdk_region_union(mRegion, pRegion->mRegion);
      }
    } else
      mRegion = gdk_region_copy(pRegion->mRegion);
  }
}

void nsRegionGTK::Union(PRInt32 aX, PRInt32 aY,
                        PRInt32 aWidth, PRInt32 aHeight)
{
  GdkRectangle grect;
  
  grect.x = aX;
  grect.y = aY;
  grect.width = aWidth;
  grect.height = aHeight;
  
  if (mRegion) {
    if (grect.width > 0 && grect.height > 0) {
      if (gdk_region_empty(mRegion)) {
        gdk_region_destroy(mRegion);
        mRegion = gdk_region_rectangle(&grect);
      } else {
        gdk_region_union_with_rect(mRegion, &grect);
      }
    }
  } else {
    mRegion = gdk_region_rectangle(&grect);
  }
}

void nsRegionGTK::Subtract(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;
  if (pRegion->mRegion) {
    if (mRegion) {
      gdk_region_subtract(mRegion, pRegion->mRegion);
    } else {
      mRegion = gdk_region_new();
      gdk_region_subtract(mRegion, pRegion->mRegion);
    }
  }
}

void nsRegionGTK::Subtract(PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  GdkRectangle rect;
  rect.x = aX;
  rect.y = aY;
  rect.width = aWidth;
  rect.height = aHeight;
  GdkRegion *tRegion = gdk_region_rectangle(&rect);

  if (mRegion) {
    gdk_region_subtract(mRegion, tRegion);
  } else {
    NS_WARNING("subtracting from a non-region?");
    mRegion = gdk_region_new();
    gdk_region_subtract(mRegion, tRegion);
  }

  gdk_region_destroy(tRegion);
}

PRBool nsRegionGTK::IsEmpty(void)
{
  if (!mRegion)
    return PR_TRUE;
  return (gdk_region_empty(mRegion));
}

PRBool nsRegionGTK::IsEqual(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;

  if (mRegion && pRegion->mRegion) {
    return(gdk_region_equal(mRegion, pRegion->mRegion));
  } else if (!mRegion && !pRegion->mRegion) {
    return PR_TRUE;
  } else if ((mRegion && !pRegion->mRegion) || (!mRegion && pRegion->mRegion)) {
    return PR_FALSE;
  }

  return PR_FALSE;
}

void nsRegionGTK::GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  if (mRegion) {
    GdkRectangle rect;

    gdk_region_get_clipbox(mRegion, &rect);

    *aX = rect.x;
    *aY = rect.y;
    *aWidth = rect.width;
    *aHeight = rect.height;
  } else {
    *aX = 0;
    *aY = 0;
    *aWidth = 0;
    *aHeight = 0;
  }
}

void nsRegionGTK::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  if (mRegion) {
    gdk_region_offset(mRegion, aXOffset, aYOffset);
  }
}

PRBool nsRegionGTK::ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (mRegion) {
    GdkOverlapType containment;
    GdkRectangle rect;
   
    rect.x = aX;
    rect.y = aY;
    rect.width = aWidth;
    rect.height = aHeight;
   
    containment = gdk_region_rect_in(mRegion, &rect);

    if (containment != GDK_OVERLAP_RECTANGLE_OUT)
      return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP nsRegionGTK::GetRects(nsRegionRectSet **aRects)
{
  nsRegionRectSet *retval;
  nsRegionRect *regionrect;

  *aRects = nsnull;

  if (!mRegion)
    return NS_OK;

  GdkRectangle *rects = nsnull;
  gint          nrects = 0;
  
  gdk_region_get_rectangles(mRegion, &rects, &nrects);

  // There are no rectangles in this region but we still need to
  // return an empty structure.
  if (!nrects) {
    retval = (nsRegionRectSet *)nsMemory::Alloc(sizeof(nsRegionRectSet));
    if (!retval)
      return NS_ERROR_OUT_OF_MEMORY;

    retval->mNumRects = 0;
    retval->mRectsLen = 0;
    retval->mArea = 0;

    *aRects = retval;

    return NS_OK;
  }

  // allocate space for our return values
  retval = (nsRegionRectSet *)
    nsMemory::Alloc(sizeof(nsRegionRectSet) +
                    (sizeof(nsRegionRect) * (nrects - 1)));
  if (!retval)
    return NS_ERROR_OUT_OF_MEMORY;

  regionrect = &retval->mRects[0];
  retval->mNumRects = nrects;
  retval->mRectsLen = nrects;

  int currect = 0;
  while (currect < nrects) {
    regionrect->x = rects[currect].x;
    regionrect->y = rects[currect].y;
    regionrect->width = rects[currect].width;
    regionrect->height = rects[currect].height;

    retval->mArea += rects[currect].width * rects[currect].height;

    currect++;
    regionrect++;
  }

  // they are allocated as one lump
  g_free(rects);

  *aRects = retval;
  return NS_OK;
}

NS_IMETHODIMP nsRegionGTK::FreeRects(nsRegionRectSet *aRects)
{
  if (nsnull != aRects)
    nsMemory::Free(aRects);

  return NS_OK;
}

NS_IMETHODIMP nsRegionGTK::GetNativeRegion(void *&aRegion) const
{
  aRegion = (void *)mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsRegionGTK::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  // cast to avoid const-ness problems on some compilers
  if (((nsRegionGTK*)this)->IsEmpty())
    aComplexity = eRegionComplexity_empty;
  else
    aComplexity = eRegionComplexity_complex;

  return NS_OK;
}

NS_IMETHODIMP nsRegionGTK::GetNumRects(PRUint32 *aRects) const
{
  if (!mRegion)
    *aRects = 0;

  GdkRectangle *rects = nsnull;
  gint nrects = 0;

  gdk_region_get_rectangles(mRegion, &rects, &nrects);

  // freed as one lump
  g_free(rects);

  *aRects = nrects;
  
  return NS_OK;
}
