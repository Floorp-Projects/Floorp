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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 */

#include "nsRegion.h"
#include "nsRect.h"
#include "xregion.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS1(nsRegion, nsIRegion)

nsRegion::nsRegion()
{
  NS_INIT_REFCNT();

  mRegion = NULL;
  mRegionType = NULLREGION;
  mData = NULL;
  mDataSize = 0;
}

nsRegion::~nsRegion()
{
  if (NULL != mRegion)
  {
    ::DeleteObject(mRegion);
    mRegion = NULL;
  }

  if (NULL != mData)
  {
    PR_Free(mData);

    mData = NULL;
    mDataSize = 0;
  }
}

NS_IMETHODIMP nsRegion::Init(void)
{
  if (NULL != mRegion) {
    ::DeleteObject(mRegion);
    FreeRects(nsnull);
  }
  mRegion = ::CreateRectRgn(0, 0, 0, 0);
  mRegionType = NULLREGION;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::Copy(nsIRegion **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsRegion::SetToRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  mRegionType = ::CombineRgn(mRegion, pRegion->mRegion, NULL, RGN_COPY);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::SetToRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  if (NULL != mRegion)
    ::DeleteObject(mRegion);

  mRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);

  if ((aWidth == 0) || (aHeight == 0))
    mRegionType = NULLREGION;
  else
    mRegionType = SIMPLEREGION;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IntersectRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_AND);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IntersectRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, mRegion, tRegion, RGN_AND);

  ::DeleteObject(tRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::UnionRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_OR);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::UnionRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, mRegion, tRegion, RGN_OR);

  ::DeleteObject(tRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::SubtractRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_DIFF);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::SubtractRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, mRegion, tRegion, RGN_DIFF);

  ::DeleteObject(tRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IsEmpty(PRBool *aResult)
{
  *aResult = (mRegionType == NULLREGION) ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IsEqual(nsIRegion *aRegion, PRBool *aResult)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  *aResult = ::EqualRgn(mRegion, pRegion->mRegion) ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::GetBoundingBox(gfx_coord *aX, gfx_coord *aY, gfx_dimension *aWidth, gfx_dimension *aHeight)
{
  RECT  bounds;

  if (mRegionType != NULLREGION)
  {
    ::GetRgnBox(mRegion, &bounds);

    *aX = bounds.left;
    *aY = bounds.top;
    *aWidth = bounds.right - bounds.left;
    *aHeight = bounds.bottom - bounds.top;
  }
  else
    *aX = *aY = *aWidth = *aHeight = 0;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::OffsetBy(gfx_coord aXOffset, gfx_coord aYOffset)
{
  ::OffsetRgn(mRegion, aXOffset, aYOffset);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::ContainsRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight, PRBool *aResult)
{
  RECT  trect;

  trect.left = aX;
  trect.top = aY;
  trect.right = aX + aWidth;
  trect.bottom = aY + aHeight;

  *aResult = ::RectInRegion(mRegion, &trect) ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::GetRects(nsRect ***aRects, PRUint32 *npoints)
{
#if 0
  nsRegionRectSet *rects;
  nsRegionRect    *rect;
  LPRECT          pRects;
  DWORD           dwCount, dwResult;
  unsigned int    num_rects;

  NS_ASSERTION(!(nsnull == aRects), "bad ptr");

  rects = *aRects;

  if (nsnull != rects)
    rects->mNumRects = 0;

  // code lifted from old winfe. MMP

  /* Get the size of the region data */
  dwCount = GetRegionData(mRegion, 0, NULL);

  NS_ASSERTION(!(dwCount == 0), "bad region");

  if (dwCount == 0)
    return NS_OK;

  if (dwCount > mDataSize)
  {
    if (NULL != mData)
      PR_Free(mData);

    mData = (LPRGNDATA)PR_Malloc(dwCount);
  }

  NS_ASSERTION(!(nsnull == mData), "failed allocation");

  if (mData == NULL)
    return NS_OK;

  dwResult = GetRegionData(mRegion, dwCount, mData);

  NS_ASSERTION(!(dwResult == 0), "get data failed");

  if (dwResult == 0)
    return NS_OK;

  if ((nsnull == rects) || (rects->mRectsLen < mData->rdh.nCount))
  {
    void *buf = PR_Realloc(rects, sizeof(nsRect) + (sizeof(nsRegionRect) * (mData->rdh.nCount - 1)));

    if (nsnull == buf)
    {
      if (nsnull != rects)
        rects->mNumRects = 0;

      return NS_OK;
    }

    rects = (nsRegionRectSet *)buf;
    rects->mRectsLen = mData->rdh.nCount;
  }

  rects->mNumRects = mData->rdh.nCount;
  rects->mArea = 0;
  rect = &rects->mRects[0];

  for (pRects = (LPRECT)mData->Buffer, num_rects = 0; 
       num_rects < mData->rdh.nCount; 
       num_rects++, pRects++, rect++)
  {
    rect->x = pRects->left;
    rect->y = pRects->top;
    rect->width = pRects->right - rect->x;
    rect->height = pRects->bottom - rect->y;

    rects->mArea += rect->width * rect->height;
  }

  *aRects = rects;

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLIMENTED;
#endif
}

NS_IMETHODIMP nsRegion::GetNumRects(PRUint32 *aRects)
{
  return NS_ERROR_NOT_IMPLIMENTED;
}
