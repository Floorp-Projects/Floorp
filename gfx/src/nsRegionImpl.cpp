/*
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
 * The Initial Developer of the Original Code is Dainis Jonitis,
 * <Dainis_Jonitis@swh-t.lv>.  Portions created by Dainis Jonitis are
 * Copyright (C) 2001 Dainis Jonitis. All Rights Reserved.
 *
 * Contributor(s):
 */

#include "nsRegion.h"
#include "nsRegionImpl.h"


nsresult nsRegionImpl::Init (void)
{
  mRegion.SetEmpty ();
  return NS_OK;
}

void nsRegionImpl::SetTo (const nsIRegion &aRegion)
{
  const nsRegionImpl* pRegion = NS_STATIC_CAST (const nsRegionImpl*, &aRegion);
  mRegion = pRegion->mRegion;
}

void nsRegionImpl::SetTo (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion = nsRect (aX, aY, aWidth, aHeight);
}

void nsRegionImpl::Intersect (const nsIRegion &aRegion)
{
  const nsRegionImpl* pRegion = NS_STATIC_CAST (const nsRegionImpl*, &aRegion);
  mRegion.And (mRegion, pRegion->mRegion);
}

void nsRegionImpl::Intersect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.And (mRegion, nsRect (aX, aY, aWidth, aHeight));
}

void nsRegionImpl::Union (const nsIRegion &aRegion)
{
  const nsRegionImpl* pRegion = NS_STATIC_CAST (const nsRegionImpl*, &aRegion);
  mRegion.Or (mRegion, pRegion->mRegion);
}

void nsRegionImpl::Union (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.Or (mRegion, nsRect (aX, aY, aWidth, aHeight));
}

void nsRegionImpl::Subtract (const nsIRegion &aRegion)
{
  const nsRegionImpl* pRegion = NS_STATIC_CAST (const nsRegionImpl*, &aRegion);
  mRegion.Sub (mRegion, pRegion->mRegion);
}

void nsRegionImpl::Subtract (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.Sub (mRegion, nsRect (aX, aY, aWidth, aHeight));
}

PRBool nsRegionImpl::IsEmpty (void)
{
  return mRegion.IsEmpty ();
}

PRBool nsRegionImpl::IsEqual (const nsIRegion &aRegion)
{
  const nsRegionImpl* pRegion = NS_STATIC_CAST (const nsRegionImpl*, &aRegion);
  return mRegion.IsEqual (pRegion->mRegion);
}

void nsRegionImpl::GetBoundingBox (PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  const nsRect& BoundRect = mRegion.GetBounds();
  *aX = BoundRect.x;
  *aY = BoundRect.y;
  *aWidth  = BoundRect.width;
  *aHeight = BoundRect.height;
}

void nsRegionImpl::Offset (PRInt32 aXOffset, PRInt32 aYOffset)
{
  mRegion.MoveBy (aXOffset, aYOffset);
}

PRBool nsRegionImpl::ContainsRect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  nsRegion TmpRegion;
  TmpRegion.And (mRegion, nsRect (aX, aY, aWidth, aHeight));
  return (!TmpRegion.IsEmpty ());
}

nsresult nsRegionImpl::GetRects (nsRegionRectSet **aRects)
{
  if (!aRects)
    return NS_ERROR_NULL_POINTER;

  nsRegionRectSet* pRegionSet = *aRects;
  PRUint32 NumRects = mRegion.GetNumRects ();

  if (!pRegionSet)                          // Not yet allocated
  {
    PRUint8* pBuf = new PRUint8 [sizeof (nsRegionRectSet) + NumRects * sizeof (nsRegionRect)];
    pRegionSet = NS_REINTERPRET_CAST (nsRegionRectSet*, pBuf);
    pRegionSet->mRectsLen = NumRects + 1;
  } else                                    // Already allocated in previous call
  {
    if (NumRects > pRegionSet->mRectsLen)   // passed array is not big enough - reallocate it.
    {
      delete [] NS_REINTERPRET_CAST (PRUint8*, pRegionSet);
      PRUint8* pBuf = new PRUint8 [sizeof (nsRegionRectSet) + NumRects * sizeof (nsRegionRect)];
      pRegionSet = NS_REINTERPRET_CAST (nsRegionRectSet*, pBuf);
      pRegionSet->mRectsLen = NumRects + 1;
    }
  }
  pRegionSet->mNumRects = NumRects;
  *aRects = pRegionSet;


  nsRegionRectIterator ri (mRegion);
  nsRegionRect* pDest = &pRegionSet->mRects [0];
  const nsRect* pSrc;

  while ((pSrc = ri.Next ()) != nsnull)
  {
    pDest->x = pSrc->x;
    pDest->y = pSrc->y;
    pDest->width  = pSrc->width;
    pDest->height = pSrc->height;

    ++pDest;
  }

  return NS_OK;
}

nsresult nsRegionImpl::FreeRects (nsRegionRectSet *aRects)
{
  if (!aRects)
    return NS_ERROR_NULL_POINTER;

  delete [] NS_REINTERPRET_CAST (PRUint8*, aRects);
  return NS_OK;
}

nsresult nsRegionImpl::GetNativeRegion (void *&aRegion) const
{
  aRegion = 0;
  return NS_OK;
}

nsresult nsRegionImpl::GetRegionComplexity (nsRegionComplexity &aComplexity) const
{
  switch (mRegion.GetNumRects ())
  {
    case 0:   aComplexity = eRegionComplexity_empty;    break;
    case 1:   aComplexity = eRegionComplexity_rect;     break;
    default:  aComplexity = eRegionComplexity_complex;  break;
  }

  return NS_OK;
}

nsresult nsRegionImpl::GetNumRects (PRUint32 *aRects) const
{
  *aRects = mRegion.GetNumRects ();
  return NS_OK;
}
