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
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsRegion.h"
#include "nsRect2.h"
#include "xregion.h"
#include "prmem.h"

Region nsRegion::copyRegion = nsnull;

NS_IMPL_ISUPPORTS1(nsRegion, nsIRegion)

nsRegion::nsRegion()
{
  NS_INIT_REFCNT();

  mRegion = nsnull;
}

nsRegion::~nsRegion()
{
  if (mRegion)
    ::XDestroyRegion(mRegion);
  mRegion = nsnull;
}

Region
nsRegion::GetCopyRegion() {
  if (!copyRegion) copyRegion = ::XCreateRegion();
  return copyRegion;
}

void
nsRegion::XCopyRegion(Region srca, Region dr_return)
{
  ::XUnionRegion(srca, GetCopyRegion(), dr_return);
}

void
nsRegion::XUnionRegionWithRect(Region srca,
                               gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight,
                               Region dr_return)
{
  XRectangle rect;

  rect.x = GFXCoordToIntRound(aX);
  rect.y = GFXCoordToIntRound(aY);
  rect.width = GFXCoordToIntRound(aWidth);
  rect.height = GFXCoordToIntRound(aHeight);

  ::XUnionRectWithRegion(&rect, srca, dr_return);
}

NS_IMETHODIMP nsRegion::Init(void)
{
  if (mRegion) {
    ::XDestroyRegion(mRegion);
    mRegion = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegion::Copy(nsIRegion **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsRegion::SetToRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = (nsRegion *)aRegion;

  if (!mRegion)
    mRegion = ::XCreateRegion();
  this->XCopyRegion(pRegion->mRegion, mRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::SetToRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  if (!mRegion)
    mRegion = ::XCreateRegion();
  this->XUnionRegionWithRect(GetCopyRegion(), aX, aY, aWidth, aHeight, mRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IntersectRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  ::XIntersectRegion(mRegion, pRegion->mRegion, mRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IntersectRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  Region tRegion = ::XCreateRegion();
  this->XUnionRegionWithRect(GetCopyRegion(), aX, aY, aWidth, aHeight, tRegion);

  ::XIntersectRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::UnionRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  if (pRegion->mRegion) {
    if (mRegion) {
      ::XUnionRegion(mRegion, pRegion->mRegion, mRegion);
    } else {
      mRegion = ::XCreateRegion();
      this->XCopyRegion(pRegion->mRegion, mRegion);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegion::UnionRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  if (!mRegion)
    mRegion = ::XCreateRegion();

  this->XUnionRegionWithRect(mRegion, aX, aY, aWidth, aHeight, mRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::SubtractRegion(nsIRegion *aRegion)
{
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);
  if (pRegion->mRegion) {
    if (!mRegion) {
      mRegion = ::XCreateRegion();
    }
    ::XSubtractRegion(mRegion, pRegion->mRegion, mRegion);
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegion::SubtractRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  Region tRegion = ::XCreateRegion();
  this->XUnionRegionWithRect(GetCopyRegion(), aX, aY, aWidth, aHeight, tRegion);

  if (!mRegion) {
    mRegion = ::XCreateRegion();
  }

  ::XSubtractRegion(mRegion, tRegion, mRegion);

  ::XDestroyRegion(tRegion);

  return NS_OK;
}

NS_IMETHODIMP nsRegion::IsEmpty(PRBool *aResult)
{
  if (!mRegion) {
    *aResult = PR_TRUE;
    return NS_OK;
  }
  *aResult = ::XEmptyRegion(mRegion);
  return NS_OK;
}

NS_IMETHODIMP nsRegion::IsEqual(nsIRegion *aRegion, PRBool *aResult)
{
  *aResult = PR_FALSE;
  nsRegion *pRegion = NS_STATIC_CAST(nsRegion*, aRegion);

  if (mRegion && pRegion->mRegion) {
    *aResult = ::XEqualRegion(mRegion, pRegion->mRegion);
  } else if (!mRegion && !pRegion->mRegion) {
    *aResult = PR_TRUE;
  } else if ((mRegion && !pRegion->mRegion) || (!mRegion && pRegion->mRegion)) {
    *aResult = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegion::GetBoundingBox(gfx_coord *aX, gfx_coord *aY, gfx_dimension *aWidth, gfx_dimension *aHeight)
{
  if (mRegion) {
    XRectangle rect;

    ::XClipBox(mRegion, &rect);

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

  return NS_OK;
}

NS_IMETHODIMP nsRegion::OffsetBy(gfx_coord aXOffset, gfx_coord aYOffset)
{
  if (mRegion) {
    ::XOffsetRegion(mRegion, GFXCoordToIntRound(aXOffset), GFXCoordToIntRound(aYOffset));
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegion::ContainsRect(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight, PRBool *aResult)
{
  *aResult = PR_FALSE;

  if (mRegion) {
    int containment = ::XRectInRegion(mRegion, 
                                      GFXCoordToIntRound(aX), GFXCoordToIntRound(aY),
                                      GFXCoordToIntRound(aWidth), GFXCoordToIntRound(aHeight));

    if (containment != RectangleOut)
      *aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRegion::GetRects(nsRect2 ***aRects, PRUint32 *npoints)
{
  if (!mRegion)
    return NS_OK;

  // XXX untested code :-)

  nsRect2 *rects = new nsRect2[mRegion->numRects];

  BOX *pbox = mRegion->rects;
  int nbox = mRegion->numRects;

  while (nbox--)
  {
    rects->SetRect(pbox->x1,
                   pbox->y1,
                   (pbox->x2 - pbox->x1),
                   (pbox->y2 - pbox->y1));
    rects++;
    pbox++;
  }

  *aRects = &rects;

  return NS_OK;
}

NS_IMETHODIMP nsRegion::GetNumRects(PRUint32 *aRects)
{
  if (!mRegion)
    *aRects = 0;

  *aRects = mRegion->numRects;

  return NS_OK;
}
