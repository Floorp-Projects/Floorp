/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsRegionWin.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionWin :: nsRegionWin()
{
  NS_INIT_REFCNT();

  mRegion = NULL;
  mRegionType = NULLREGION;
}

nsRegionWin :: ~nsRegionWin()
{
  mRegion = NULL;
}

NS_IMPL_QUERY_INTERFACE(nsRegionWin, kRegionIID)
NS_IMPL_ADDREF(nsRegionWin)
NS_IMPL_RELEASE(nsRegionWin)

nsresult nsRegionWin :: Init(void)
{
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

  mRegionType = ::CombineRgn(mRegion, pRegion->mRegion, mRegion, RGN_AND);
}

void nsRegionWin :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, tRegion, mRegion, RGN_AND);

  ::DeleteObject(tRegion);
}

void nsRegionWin :: Union(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  mRegionType = ::CombineRgn(mRegion, pRegion->mRegion, mRegion, RGN_OR);
}

void nsRegionWin :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  HRGN tRegion;

  tRegion = ::CreateRectRgn(aX, aY, aX + aWidth, aY + aHeight);
  mRegionType = ::CombineRgn(mRegion, tRegion, mRegion, RGN_OR);

  ::DeleteObject(tRegion);
}

void nsRegionWin :: Subtract(const nsIRegion &aRegion)
{
  nsRegionWin *pRegion = (nsRegionWin *)&aRegion;

  mRegionType = ::CombineRgn(mRegion, mRegion, pRegion->mRegion, RGN_DIFF);
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

PRBool nsRegionWin :: ForEachRect(nsRectInRegionFunc *func, void *closure)
{
  return PR_FALSE;
}

HRGN nsRegionWin :: GetHRGN(void)
{
  return mRegion;
}
