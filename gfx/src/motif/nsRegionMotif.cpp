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

#include "nsRegionMotif.h"

nsRegionMotif :: nsRegionMotif()
{
  NS_INIT_REFCNT();

  mRegion = nsnull;
  mRegionType = eRegionComplexity_empty;
}

nsRegionMotif :: ~nsRegionMotif()
{
  if (mRegion)
    ::XDestroyRegion(mRegion);
  mRegion = nsnull;
}

NS_IMPL_ISUPPORTS1(nsRegionMotif, nsIRegion)

nsresult nsRegionMotif :: Init(void)
{
  mRegion = ::XCreateRegion();
  mRegionType = eRegionComplexity_empty;

  return NS_OK;
}

void nsRegionMotif :: SetTo(const nsIRegion &aRegion)
{
  nsRegionMotif * pRegion = (nsRegionMotif *)&aRegion;

  SetRegionEmpty();

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();
}

void nsRegionMotif :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
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

void nsRegionMotif :: Intersect(const nsIRegion &aRegion)
{
  nsRegionMotif * pRegion = (nsRegionMotif *)&aRegion;

  ::XIntersectRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();
}

void nsRegionMotif :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  ::XIntersectRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();

}

void nsRegionMotif :: Union(const nsIRegion &aRegion)
{
  nsRegionMotif * pRegion = (nsRegionMotif *)&aRegion;

  ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();

}

void nsRegionMotif :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  ::XUnionRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();

}

void nsRegionMotif :: Subtract(const nsIRegion &aRegion)
{
  nsRegionMotif * pRegion = (nsRegionMotif *)&aRegion;

  ::XSubtractRegion(mRegion, pRegion->mRegion, mRegion);

  SetRegionType();

}

void nsRegionMotif :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  ::XSubtractRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  SetRegionType();

}

PRBool nsRegionMotif :: IsEmpty(void)
{
  if (mRegionType == eRegionComplexity_empty)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool nsRegionMotif :: IsEqual(const nsIRegion &aRegion)
{
  nsRegionMotif * pRegion = (nsRegionMotif *)&aRegion;

  return(::XEqualRegion(mRegion, pRegion->mRegion));

}

void nsRegionMotif :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  XRectangle rect;

  ::XClipBox(mRegion, &rect);

  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;
}

void nsRegionMotif :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  ::XOffsetRegion(mRegion, aXOffset, aYOffset);
}

PRBool nsRegionMotif :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PRInt32 containment;

  containment = ::XRectInRegion(mRegion, aX, aY, aWidth, aHeight);

  if (containment == RectangleIn)
    return PR_TRUE;
  else
    return PR_FALSE;

}


NS_IMETHODIMP nsRegionMotif :: GetRects(nsRegionRectSet **aRects)
{
  return NS_OK;
}

NS_IMETHODIMP nsRegionMotif :: FreeRects(nsRegionRectSet *aRects)
{
  return NS_OK;
}

NS_IMETHODIMP nsRegionMotif :: GetNativeRegion(void *&aRegion) const
{
  aRegion = (void *)mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsRegionMotif :: GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  aComplexity = mRegionType;
  return NS_OK;
}

void nsRegionMotif :: SetRegionType()
{
  if (::XEmptyRegion(mRegion) == True)
    mRegionType = eRegionComplexity_empty;
  else
    mRegionType = eRegionComplexity_rect ;
}

void nsRegionMotif :: SetRegionEmpty()
{
  ::XDestroyRegion(mRegion);
  mRegion = ::XCreateRegion();
}

Region nsRegionMotif :: CreateRectRegion(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
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








