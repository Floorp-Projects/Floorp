/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsRegionWin.h"
#include "prmem.h"

nsRegionWin :: nsRegionWin()
{
  NS_INIT_REFCNT();

  mRegion = NULL;
  mRegionType = NULLREGION;
  mData = NULL;
  mDataSize = 0;
}

nsRegionWin :: ~nsRegionWin()
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

NS_IMPL_ISUPPORTS1(nsRegionWin, nsIRegion)

nsresult nsRegionWin :: Init(void)
{
  if (NULL != mRegion) {
    ::DeleteObject(mRegion);
    FreeRects(nsnull);
  }
  mRegion = ::CreateRectRgn(0, 0, 0, 0);
  mRegionType = NULLREGION;

  return NS_OK;
}

void nsRegionWin :: SetTo(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  mRegionType = ::CombineRgn(mRegion, pRegion->mRegion, NULL, RGN_COPY);
}

void nsRegionWin :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (NULL != mRegion)
    ::DeleteObject(mRegion);

  mRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);

  if ((aWidth == 0) || (aHeight == 0))
    mRegionType = NULLREGION;
  else
    mRegionType = SIMPLEREGION;
}

void nsRegionWin :: Intersect(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_AND);
}

void nsRegionWin :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, mRegion, tRegion, RGN_AND);

  ::DeleteObject(tRegion);
}

void nsRegionWin :: Union(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_OR);
}

void nsRegionWin :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, mRegion, tRegion, RGN_OR);

  ::DeleteObject(tRegion);
}

void nsRegionWin :: Subtract(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_DIFF);
}

void nsRegionWin :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, mRegion, tRegion, RGN_DIFF);

  ::DeleteObject(tRegion);
}

PRBool nsRegionWin :: IsEmpty(void)
{
  return (mRegionType == NULLREGION) ? PR_TRUE : PR_FALSE;
}

PRBool nsRegionWin :: IsEqual(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  return ::EqualRgn(mRegion, pRegion->mRegion) ? PR_TRUE : PR_FALSE;
}

void nsRegionWin :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
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
}

void nsRegionWin :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  ::OffsetRgn(mRegion, aXOffset, aYOffset);
}

PRBool nsRegionWin :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  RECT  trect;

  trect.left = aX;
  trect.top = aY;
  trect.right = aX + aWidth;
  trect.bottom = aY + aHeight;

  return ::RectInRegion(mRegion, &trect) ? PR_TRUE : PR_FALSE;
}

NS_IMETHODIMP nsRegionWin :: GetRects(nsRegionRectSet **aRects)
{
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
    void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) + (sizeof(nsRegionRect) * (mData->rdh.nCount - 1)));

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
}

NS_IMETHODIMP nsRegionWin :: FreeRects(nsRegionRectSet *aRects)
{
  if (nsnull != aRects)
    PR_Free((void *)aRects);

  if (NULL != mData)
  {
    PR_Free(mData);

    mData = NULL;
    mDataSize = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegionWin :: GetNativeRegion(void *&aRegion) const
{
  aRegion = (void *)mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsRegionWin :: GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  switch (mRegionType)
  {
    case NULLREGION:
      aComplexity = eRegionComplexity_empty;
      break;

    case SIMPLEREGION:
      aComplexity = eRegionComplexity_rect;
      break;

    default:
    case COMPLEXREGION:
      aComplexity = eRegionComplexity_complex;
      break;
  }

  return NS_OK;
}
