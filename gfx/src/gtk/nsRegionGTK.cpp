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

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include "nsRegionGTK.h"
#include "xregion.h"
#include "prmem.h"

#ifdef DEBUG_REGIONS
static int nRegions;
#endif

GdkRegion *nsRegionGTK::copyRegion = nsnull;


nsRegionGTK::nsRegionGTK()
{
  NS_INIT_REFCNT();

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
    ::gdk_region_destroy(mRegion);
  mRegion = nsnull;
}

NS_IMPL_ISUPPORTS1(nsRegionGTK, nsIRegion)


/* static */ void
nsRegionGTK::Shutdown()
{
  if (copyRegion) {
    gdk_region_destroy(copyRegion);
    copyRegion = nsnull;
  }
}

GdkRegion *
nsRegionGTK::GetCopyRegion() {
  if (!copyRegion) copyRegion = gdk_region_new();
  return copyRegion;
}



GdkRegion *
nsRegionGTK::gdk_region_copy(GdkRegion *region)
{
  return gdk_regions_union(region, GetCopyRegion());
}

GdkRegion *
nsRegionGTK::gdk_region_from_rect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  GdkRectangle grect;

  grect.x = aX;
  grect.y = aY;
  grect.width = aWidth;
  grect.height = aHeight;

  return ::gdk_region_union_with_rect(GetCopyRegion(), &grect);
}

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

  mRegion = gdk_region_from_rect(aX, aY, aWidth, aHeight);
}

void nsRegionGTK::Intersect(const nsIRegion &aRegion)
{
  nsRegionGTK * pRegion = (nsRegionGTK *)&aRegion;

  GdkRegion *nRegion = ::gdk_regions_intersect(mRegion, pRegion->mRegion);
  ::gdk_region_destroy(mRegion);
  mRegion = nRegion;
}

void nsRegionGTK::Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  GdkRegion *tRegion = gdk_region_from_rect(aX, aY, aWidth, aHeight);

  GdkRegion *nRegion = ::gdk_regions_intersect(mRegion, tRegion);
  ::gdk_region_destroy(tRegion);
  ::gdk_region_destroy(mRegion);
  mRegion = nRegion;
}

void nsRegionGTK::Union(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;

  if (pRegion->mRegion && !::gdk_region_empty(pRegion->mRegion)) {
    if (mRegion) {
      if (::gdk_region_empty(mRegion)) {
        ::gdk_region_destroy(mRegion);
        mRegion = gdk_region_copy(pRegion->mRegion);
      } else {
        GdkRegion *nRegion = ::gdk_regions_union(mRegion, pRegion->mRegion);
        ::gdk_region_destroy(mRegion);
        mRegion = nRegion;
      }
    } else
      mRegion = gdk_region_copy(pRegion->mRegion);
  }
}

void nsRegionGTK::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (mRegion) {
    GdkRectangle grect;

    grect.x = aX;
    grect.y = aY;
    grect.width = aWidth;
    grect.height = aHeight;

    if (grect.width > 0 && grect.height > 0) {
      if (::gdk_region_empty(mRegion)) {
        ::gdk_region_destroy(mRegion);
        mRegion = gdk_region_from_rect(aX, aY, aWidth, aHeight);
      } else {
        GdkRegion *nRegion = ::gdk_region_union_with_rect(mRegion, &grect);
        ::gdk_region_destroy(mRegion);
        mRegion = nRegion;
      }
    }
  } else {
    mRegion = gdk_region_from_rect(aX, aY, aWidth, aHeight);
  }
}

void nsRegionGTK::Subtract(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;
  if (pRegion->mRegion) {
    if (mRegion) {
      GdkRegion *nRegion = ::gdk_regions_subtract(mRegion, pRegion->mRegion);
      ::gdk_region_destroy(mRegion);
      mRegion = nRegion;
    } else {
      mRegion = ::gdk_regions_subtract(GetCopyRegion(), pRegion->mRegion);
    }
  }
}

void nsRegionGTK::Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (mRegion) {
    GdkRegion *tRegion = gdk_region_from_rect(aX, aY, aWidth, aHeight);
    
    GdkRegion *nRegion = ::gdk_regions_subtract(mRegion, tRegion);
    ::gdk_region_destroy(mRegion);
    ::gdk_region_destroy(tRegion);
    mRegion = nRegion;
  } else {
    GdkRegion *tRegion = gdk_region_from_rect(aX, aY, aWidth, aHeight);
    mRegion = ::gdk_regions_subtract(GetCopyRegion(), tRegion);
    ::gdk_region_destroy(tRegion);
  }
}

PRBool nsRegionGTK::IsEmpty(void)
{
  if (!mRegion)
    return PR_TRUE;
  return (::gdk_region_empty(mRegion));
}

PRBool nsRegionGTK::IsEqual(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;

  if (mRegion && pRegion->mRegion) {
    return(::gdk_region_equal(mRegion, pRegion->mRegion));
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

    ::gdk_region_get_clipbox(mRegion, &rect);

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
    ::gdk_region_offset(mRegion, aXOffset, aYOffset);
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
   
    containment = ::gdk_region_rect_in(mRegion, &rect);

    if (containment != GDK_OVERLAP_RECTANGLE_OUT)
      return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP nsRegionGTK::GetRects(nsRegionRectSet **aRects)
{

  *aRects = nsnull;

  if (!mRegion)
    return NS_OK;

  nsRegionRectSet   *rects = nsnull;
  GdkRegionPrivate  *priv = nsnull;
  Region            pRegion;
  int               nbox = 0;
  BOX               *pbox = nsnull;
  nsRegionRect      *rect = nsnull;

  priv = (GdkRegionPrivate *)mRegion;
  pRegion = priv->xregion;
  pbox = pRegion->rects;
  nbox = pRegion->numRects;

  NS_ASSERTION(!(nsnull == aRects), "bad ptr");

  //code lifted from old xfe. MMP

  rects = *aRects;

  if ((nsnull == rects) || (rects->mRectsLen < (PRUint32)nbox))
  {
    void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) + (sizeof(nsRegionRect) * (nbox - 1)));

    if (nsnull == buf)
    {
      if (nsnull != rects)
        rects->mNumRects = 0;

      return NS_OK;
    }

    rects = (nsRegionRectSet *)buf;
    rects->mRectsLen = nbox;
  }

  rects->mNumRects = nbox;
  rects->mArea = 0;
  rect = &rects->mRects[0];

  while (nbox--)
  {
    rect->x = pbox->x1;
    rect->width = (pbox->x2 - pbox->x1);
    rect->y = pbox->y1;
    rect->height = (pbox->y2 - pbox->y1);

    rects->mArea += rect->width * rect->height;

    pbox++;
    rect++;
  }

  *aRects = rects;

  return NS_OK;
}

NS_IMETHODIMP nsRegionGTK::FreeRects(nsRegionRectSet *aRects)
{
  if (nsnull != aRects)
    PR_Free((void *)aRects);

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

void nsRegionGTK::SetRegionEmpty()
{
  if (!IsEmpty()) {
    ::gdk_region_destroy(mRegion);
  }
}

NS_IMETHODIMP nsRegionGTK::GetNumRects(PRUint32 *aRects) const
{
  if (!mRegion)
    *aRects = 0;

  GdkRegionPrivate  *priv = (GdkRegionPrivate *)mRegion;
  Region pRegion = priv->xregion;

  *aRects = pRegion->numRects;

  return NS_OK;
}
