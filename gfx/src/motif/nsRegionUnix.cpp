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

#include "nsRegionUnix.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionUnix :: nsRegionUnix()
{
  NS_INIT_REFCNT();

  mRegion = nsnull;
  mRegionType = eRegionType_empty;
}

nsRegionUnix :: ~nsRegionUnix()
{
  if (mRegion)
    ::XDestroyRegion(mRegion);
  mRegion = nsnull;
}

NS_IMPL_QUERY_INTERFACE(nsRegionUnix, kRegionIID)
NS_IMPL_ADDREF(nsRegionUnix)
NS_IMPL_RELEASE(nsRegionUnix)

nsresult nsRegionUnix :: Init(void)
{
  mRegion = ::XCreateRegion();
  mRegionType = eRegionType_empty;

  return NS_OK;
}

void nsRegionUnix :: SetTo(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  SetRegionEmpty();

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();
}

void nsRegionUnix :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

  SetRegionEmpty();

  XRectangle xrect;

  xrect.x = aX;
  xrect.y = aY;
  xrect.width = aWidth;
  xrect.height = aHeight;

  ::XUnionRectWithRegion(&xrect, mRegion, mRegion);

  SetRegionType();
}

void nsRegionUnix :: Intersect(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  ::XIntersectRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();
}

void nsRegionUnix :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  ::XIntersectRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();

}

void nsRegionUnix :: Union(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();

}

void nsRegionUnix :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  ::XUnionRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();

}

void nsRegionUnix :: Subtract(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  ::XSubtractRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();

}

void nsRegionUnix :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  ::XSubtractRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();

}

PRBool nsRegionUnix :: IsEmpty(void)
{
  if (mRegionType == eRegionType_empty)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool nsRegionUnix :: IsEqual(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  return(::XEqualRegion(mRegion, pRegion->mRegion));

}

void nsRegionUnix :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  XRectangle rect;

  ::XClipBox(mRegion, &rect);

  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;
}

void nsRegionUnix :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  ::XOffsetRegion(mRegion, aXOffset, aYOffset);
}

PRBool nsRegionUnix :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PRInt32 containment;

  containment = ::XRectInRegion(mRegion, aX, aY, aWidth, aHeight);

  if (containment == RectangleIn)
    return PR_TRUE;
  else
    return PR_FALSE;

}

PRBool nsRegionUnix :: ForEachRect(nsRectInRegionFunc *func, void *closure)
{
  return PR_FALSE;
}


Region nsRegionUnix :: GetXRegion(void)
{
  return (mRegion);
}

void nsRegionUnix :: SetRegionType()
{
  if (::XEmptyRegion(mRegion) == True)
    mRegionType = eRegionType_empty;
  else
    mRegionType = eRegionType_rect ;
}

void nsRegionUnix :: SetRegionEmpty()
{
  ::XDestroyRegion(mRegion);
  mRegion = ::XCreateRegion();
}

Region nsRegionUnix :: CreateRectRegion(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region r = ::XCreateRegion();

  XRectangle xrect;

  xrect.x = aX;
  xrect.y = aY;
  xrect.width = aWidth;
  xrect.height = aHeight;

  ::XUnionRectWithRegion(&xrect, r, r);

  return (r);
}








