/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are
 * Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Stuart Parmenter <pavlov@pavlov.net>
 *    Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

#include "nsCairoRegion.h"

NS_IMPL_ISUPPORTS1(nsCairoRegion, nsIRegion)

nsCairoRegion::nsCairoRegion() 
{  
  NS_INIT_ISUPPORTS();
}

nsresult nsCairoRegion::Init (void)
{
  mRegion.SetEmpty();
  return NS_OK;
}

void nsCairoRegion::SetTo (const nsIRegion &aRegion)
{
  const nsCairoRegion* pRegion = NS_STATIC_CAST (const nsCairoRegion*, &aRegion);
  mRegion = pRegion->mRegion;
}

void nsCairoRegion::SetTo (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion = nsRect (aX, aY, aWidth, aHeight);
}

void nsCairoRegion::Intersect (const nsIRegion &aRegion)
{
  const nsCairoRegion* pRegion = NS_STATIC_CAST (const nsCairoRegion*, &aRegion);
  mRegion.And (mRegion, pRegion->mRegion);
}

void nsCairoRegion::Intersect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.And (mRegion, nsRect (aX, aY, aWidth, aHeight));
}

void nsCairoRegion::Union (const nsIRegion &aRegion)
{
  const nsCairoRegion* pRegion = NS_STATIC_CAST (const nsCairoRegion*, &aRegion);
  mRegion.Or (mRegion, pRegion->mRegion);
}

void nsCairoRegion::Union (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.Or (mRegion, nsRect (aX, aY, aWidth, aHeight));
}

void nsCairoRegion::Subtract (const nsIRegion &aRegion)
{
  const nsCairoRegion* pRegion = NS_STATIC_CAST (const nsCairoRegion*, &aRegion);
  mRegion.Sub (mRegion, pRegion->mRegion);
}

void nsCairoRegion::Subtract (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.Sub (mRegion, nsRect (aX, aY, aWidth, aHeight));
}

PRBool nsCairoRegion::IsEmpty (void)
{
  return mRegion.IsEmpty ();
}

PRBool nsCairoRegion::IsEqual (const nsIRegion &aRegion)
{
  const nsCairoRegion* pRegion = NS_STATIC_CAST (const nsCairoRegion*, &aRegion);
  return mRegion.IsEqual (pRegion->mRegion);
}

void nsCairoRegion::GetBoundingBox (PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  nsRect BoundRect;
  BoundRect = mRegion.GetBounds();
  *aX = BoundRect.x;
  *aY = BoundRect.y;
  *aWidth  = BoundRect.width;
  *aHeight = BoundRect.height;
}

void nsCairoRegion::Offset (PRInt32 aXOffset, PRInt32 aYOffset)
{
  mRegion.MoveBy (aXOffset, aYOffset);
}

PRBool nsCairoRegion::ContainsRect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  nsRegion TmpRegion;
  TmpRegion.And (mRegion, nsRect (aX, aY, aWidth, aHeight));
  return (!TmpRegion.IsEmpty ());
}

NS_IMETHODIMP
nsCairoRegion::GetRects (nsRegionRectSet **aRects)
{
  if (!aRects)
    return NS_ERROR_NULL_POINTER;

  nsRegionRectSet* pRegionSet = *aRects;
  PRUint32 NumRects = mRegion.GetNumRects ();

  if (pRegionSet == nsnull)                 // Not yet allocated
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

  while ((pSrc = ri.Next ()))
  {
    pDest->x = pSrc->x;
    pDest->y = pSrc->y;
    pDest->width  = pSrc->width;
    pDest->height = pSrc->height;

    pDest++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCairoRegion::FreeRects (nsRegionRectSet *aRects)
{
  if (!aRects)
    return NS_ERROR_NULL_POINTER;

  delete [] NS_REINTERPRET_CAST (PRUint8*, aRects);
  return NS_OK;
}

NS_IMETHODIMP
nsCairoRegion::GetNativeRegion (void *&aRegion) const
{
  aRegion = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsCairoRegion::GetRegionComplexity (nsRegionComplexity &aComplexity) const
{
  switch (mRegion.GetNumRects ())
  {
    case 0:   aComplexity = eRegionComplexity_empty;    break;
    case 1:   aComplexity = eRegionComplexity_rect;     break;
    default:  aComplexity = eRegionComplexity_complex;  break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCairoRegion::GetNumRects (PRUint32 *aRects) const
{
  *aRects = mRegion.GetNumRects ();
  return NS_OK;
}
