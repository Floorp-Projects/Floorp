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

#include "nsRegionMac.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionMac :: nsRegionMac()
{
  NS_INIT_REFCNT();

/*  mRegion = nsnull;
  mRegionType = eRegionType_empty;*/
}

nsRegionMac :: ~nsRegionMac()
{
/*  if (mRegion)
    ::XDestroyRegion(mRegion);
  mRegion = nsnull;*/
}

NS_IMPL_QUERY_INTERFACE(nsRegionMac, kRegionIID)
NS_IMPL_ADDREF(nsRegionMac)
NS_IMPL_RELEASE(nsRegionMac)

nsresult nsRegionMac :: Init(void)
{
/*  mRegion = ::XCreateRegion();
  mRegionType = eRegionType_empty;*/

  return NS_OK;
}

void nsRegionMac :: SetTo(const nsIRegion &aRegion)
{
/*  nsRegionMac * pRegion = (nsRegionMac *)&aRegion;

  SetRegionEmpty();

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();*/
}

void nsRegionMac :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
/*
  SetRegionEmpty();

  XRectangle xrect;

  xrect.x = aX;
  xrect.y = aY;
  xrect.width = aWidth;
  xrect.height = aHeight;

  ::XUnionRectWithRegion(&xrect, mRegion, mRegion);

  SetRegionType();*/
}

void nsRegionMac :: Intersect(const nsIRegion &aRegion)
{
/*  nsRegionMac * pRegion = (nsRegionMac *)&aRegion;

  ::XIntersectRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();*/
}

void nsRegionMac :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
/*  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  ::XIntersectRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();*/

}

void nsRegionMac :: Union(const nsIRegion &aRegion)
{
/*  nsRegionMac * pRegion = (nsRegionMac *)&aRegion;

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();*/

}

void nsRegionMac :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

/*  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  ::XUnionRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();*/

}

void nsRegionMac :: Subtract(const nsIRegion &aRegion)
{
/*  nsRegionMac * pRegion = (nsRegionMac *)&aRegion;

  ::XSubtractRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();*/

}

void nsRegionMac :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
 /* Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  ::XSubtractRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();*/

}

PRBool nsRegionMac :: IsEmpty(void)
{
/*  if (mRegionType == eRegionType_empty)
    return PR_TRUE;*/

  return PR_FALSE;
}

PRBool nsRegionMac :: IsEqual(const nsIRegion &aRegion)
{
/*  nsRegionMac * pRegion = (nsRegionMac *)&aRegion;

  return(::XEqualRegion(mRegion, pRegion->mRegion));*/
  return PR_FALSE;

}

void nsRegionMac :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
/*  XRectangle rect;

  ::XClipBox(mRegion, &rect);

  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;*/
}

void nsRegionMac :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  //::XOffsetRegion(mRegion, aXOffset, aYOffset);
}

PRBool nsRegionMac :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  /*PRInt32 containment;

  containment = ::XRectInRegion(mRegion, aX, aY, aWidth, aHeight);

  if (containment == RectangleIn)
    return PR_TRUE;
  else*/
    return PR_FALSE;

}

PRBool nsRegionMac :: ForEachRect(nsRectInRegionFunc *func, void *closure)
{
  return PR_FALSE;
}


/*Region nsRegionMac :: GetXRegion(void)
{
  return (mRegion);
}*/

/*void nsRegionMac :: SetRegionType()
{
  if (::XEmptyRegion(mRegion) == True)
    mRegionType = eRegionType_empty;
  else
    mRegionType = eRegionType_rect ;
}
*/

/*void nsRegionMac :: SetRegionEmpty()
{
  ::XDestroyRegion(mRegion);
  mRegion = ::XCreateRegion();
}*/

 /*Region nsRegionMac :: CreateRectRegion(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
 Region r = ::XCreateRegion();

  XRectangle xrect;

  xrect.x = aX;
  xrect.y = aY;
  xrect.width = aWidth;
  xrect.height = aHeight;

  ::XUnionRectWithRegion(&xrect, r, r);

  return (r);
}*/








