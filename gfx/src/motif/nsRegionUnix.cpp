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

  ::XDestroyRegion(mRegion);
  mRegion = ::XCreateRegion();

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  mRegionType = eRegionType_rect ; // XXX: Should probably set to complex
}

void nsRegionUnix :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  ::XDestroyRegion(mRegion);
  mRegion = ::XCreateRegion();

  ::XOffsetRegion(mRegion, aX, aY);
  ::XShrinkRegion(mRegion, -aWidth, -aHeight);

  mRegionType = eRegionType_rect ;
}

void nsRegionUnix :: Intersect(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  ::XIntersectRegion(mRegion, pRegion->mRegion, mRegion);

  mRegionType = eRegionType_rect ;
}

void nsRegionUnix :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion;

  tRegion = ::XCreateRegion();

  ::XOffsetRegion(tRegion, aX, aY);
  ::XShrinkRegion(tRegion, -aWidth, -aHeight);
  
  ::XIntersectRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  mRegionType = eRegionType_rect ;
}

void nsRegionUnix :: Union(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  mRegionType = eRegionType_rect ;
}

void nsRegionUnix :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion;

  tRegion = ::XCreateRegion();

  ::XOffsetRegion(tRegion, aX, aY);
  ::XShrinkRegion(tRegion, -aWidth, -aHeight);
  
  ::XUnionRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  mRegionType = eRegionType_rect ;
}

void nsRegionUnix :: Subtract(const nsIRegion &aRegion)
{
  nsRegionUnix * pRegion = (nsRegionUnix *)&aRegion;

  ::XSubtractRegion(mRegion, pRegion->mRegion, mRegion);

  mRegionType = eRegionType_rect ;
}
void nsRegionUnix :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion;

  tRegion = ::XCreateRegion();

  ::XOffsetRegion(tRegion, aX, aY);
  ::XShrinkRegion(tRegion, -aWidth, -aHeight);
  
  ::XSubtractRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  mRegionType = eRegionType_rect ;
}

PRBool nsRegionUnix :: IsEmpty(void)
{
  return (::XEmptyRegion(mRegion));
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
