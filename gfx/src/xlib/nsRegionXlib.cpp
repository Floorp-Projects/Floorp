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
 *    Stuart Parmenter <pavlov@netscape.com>
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

#include "prmem.h"
#include "nsRegionXlib.h"
#include "xregion.h"

// #define DEBUG_REGIONS 1

#ifdef DEBUG_REGIONS
static int nRegions;
#endif

Region nsRegionXlib::copyRegion = 0;

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionXlib::nsRegionXlib()
{
#ifdef DEBUG_REGIONS
    ++nRegions;
      printf("REGIONS+ = %i\n", nRegions);
#endif

  NS_INIT_REFCNT();

  mRegion = nsnull;
}

nsRegionXlib::~nsRegionXlib()
{
#ifdef DEBUG_REGIONS
  --nRegions;
  printf("REGIONS- = %i\n", nRegions);
#endif
  
  if (mRegion)
    ::XDestroyRegion(mRegion);
  mRegion = nsnull;
}

NS_IMPL_ISUPPORTS1(nsRegionXlib, nsIRegion)

Region
nsRegionXlib::GetCopyRegion()
{
  if (!copyRegion)
    copyRegion = ::XCreateRegion();
  return copyRegion;
}

Region
nsRegionXlib::xlib_region_copy(Region region)
{
  Region nRegion;
  nRegion = XCreateRegion();

  XUnionRegion(region, GetCopyRegion(), nRegion);

  return nRegion;
}

Region nsRegionXlib::xlib_region_from_rect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  XRectangle rect;
  Region nRegion;

  rect.x = aX;
  rect.y = aY;
  rect.width = aWidth;
  rect.height = aHeight;

  nRegion = XCreateRegion();

  XUnionRectWithRegion(&rect, GetCopyRegion(), nRegion);

  return nRegion;
}

nsresult
nsRegionXlib::Init()
{
  if (mRegion) {
    ::XDestroyRegion(mRegion);
    mRegion = nsnull;
  }

  return NS_OK;
}

void
nsRegionXlib::SetTo(const nsIRegion &aRegion)
{
  Init();
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;

  mRegion = xlib_region_copy(pRegion->mRegion);
}

void
nsRegionXlib::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Init();
  mRegion = xlib_region_from_rect(aX, aY, aWidth, aHeight);
}

void
nsRegionXlib::Intersect(const nsIRegion &aRegion)
{
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;
  
  Region nRegion = XCreateRegion();
  ::XIntersectRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = xlib_region_from_rect(aX, aY, aWidth, aHeight);
  
  Region nRegion = XCreateRegion();

  ::XIntersectRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(tRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Union(const nsIRegion &aRegion)
{
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;

  if (pRegion->mRegion && !::XEmptyRegion(pRegion->mRegion)) {
    if (mRegion) {
      if (::XEmptyRegion(mRegion)) {
        ::XDestroyRegion(mRegion);
        mRegion = xlib_region_copy(pRegion->mRegion);
      } else {
        Region nRegion = ::XCreateRegion();
        ::XUnionRegion(mRegion, pRegion->mRegion, nRegion);
        ::XDestroyRegion(mRegion);
        mRegion = nRegion;
      }
    } else
      mRegion = xlib_region_copy(pRegion->mRegion);
  }
}

void
nsRegionXlib::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (mRegion) {
    XRectangle rect;
    
    rect.x = aX;
    rect.y = aY;
    rect.width = aWidth;
    rect.height = aHeight;

    if (rect.width > 0 && rect.height > 0) {
      if (::XEmptyRegion(mRegion)) {
        ::XDestroyRegion(mRegion);
        mRegion = xlib_region_from_rect(aX, aY, aWidth, aHeight);
      } else {
        Region nRegion = ::XCreateRegion();
        ::XUnionRectWithRegion(&rect, mRegion, nRegion);
        ::XDestroyRegion(mRegion);
        mRegion = nRegion;
      }
    }
  } else {
    mRegion = xlib_region_from_rect(aX, aY, aWidth, aHeight);
  }
}

void
nsRegionXlib::Subtract(const nsIRegion &aRegion)
{
#ifdef DEBUG_REGIONS
  printf("nsRegionXlib::Subtract ");
#endif
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;

  if (pRegion->mRegion) {
    if (mRegion) {
#ifdef DEBUG_REGIONS
    printf("-");
#endif
      Region nRegion = ::XCreateRegion();
      ::XSubtractRegion(mRegion, pRegion->mRegion, nRegion);
      ::XDestroyRegion(mRegion);
      mRegion = nRegion;
    } else {
#ifdef DEBUG_REGIONS
    printf("+");
#endif
      mRegion = ::XCreateRegion();
      ::XSubtractRegion(GetCopyRegion(), pRegion->mRegion, mRegion);
    }
  }
#ifdef DEBUG_REGIONS
    printf("\n");
#endif
}

void
nsRegionXlib::Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  if (mRegion) {
    Region tRegion = xlib_region_from_rect(aX, aY, aWidth, aHeight);

    Region nRegion = ::XCreateRegion();
    ::XSubtractRegion(mRegion, tRegion, nRegion);
    ::XDestroyRegion(mRegion);
    ::XDestroyRegion(tRegion);
    mRegion = nRegion;
  } else {
    Region tRegion = xlib_region_from_rect(aX, aY, aWidth, aHeight);
    mRegion = XCreateRegion();
    ::XSubtractRegion(GetCopyRegion(), tRegion, mRegion);
    ::XDestroyRegion(tRegion);
  }
}

PRBool
nsRegionXlib::IsEmpty(void)
{
  if (!mRegion)
    return PR_TRUE;
  return ::XEmptyRegion(mRegion);
}

PRBool
nsRegionXlib::IsEqual(const nsIRegion &aRegion)
{
  nsRegionXlib *pRegion = (nsRegionXlib *)&aRegion;

  if (mRegion && pRegion->mRegion) {
    return ::XEqualRegion(mRegion, pRegion->mRegion);
  } else if (!mRegion && !pRegion->mRegion) {
    return PR_TRUE;
  } else if ((mRegion && !pRegion->mRegion) ||
      (!mRegion && pRegion->mRegion)) {
    return PR_FALSE;
  }

  return PR_FALSE;
}

void
nsRegionXlib::GetBoundingBox(PRInt32 *aX, PRInt32 *aY,
                             PRInt32 *aWidth, PRInt32 *aHeight)
{
  if (mRegion) {
    XRectangle r;

    ::XClipBox(mRegion, &r);

    *aX = r.x;
    *aY = r.y;
    *aWidth = r.width;
    *aHeight = r.height;
  } else {
    *aX = 0;
    *aY = 0;
    *aWidth = 0;
    *aHeight = 0;
  }
}

void
nsRegionXlib::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  if (mRegion) {
    ::XOffsetRegion(mRegion, aXOffset, aYOffset);
  }
}

PRBool
nsRegionXlib::ContainsRect(PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  return (::XRectInRegion(mRegion, aX, aY, aWidth, aHeight) == RectangleIn) ?
    PR_TRUE : PR_FALSE;
}

NS_IMETHODIMP
nsRegionXlib::GetRects(nsRegionRectSet **aRects)
{
  *aRects = nsnull;

  if (!mRegion)
    return NS_OK;

  nsRegionRectSet   *rects;
  int               nbox;
  BOX               *pbox;
  nsRegionRect      *rect;

  NS_ASSERTION(!(nsnull == aRects), "bad ptr");
 
  //code lifted from old xfe. MMP

  pbox = mRegion->rects;
  nbox = mRegion->numRects;
 
  rects = *aRects;

  if ((nsnull == rects) || (rects->mRectsLen < (PRUint32)nbox)) {
    void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) +
                           (sizeof(nsRegionRect) * (nbox - 1)));

    if (nsnull == buf) {
      if (nsnull != rects)
        rects->mNumRects = 0;
 
      return NS_OK;
    }

    rects = (nsRegionRectSet *)buf;
    rects->mRectsLen = nbox;
  }

  rects->mNumRects = nbox;
  rects->mArea = 0;
  rect = &rects->mRects[0];

  while (nbox--) {
    rect->x = pbox->x1;
    rect->width = (pbox->x2 - pbox->x1);
    rect->y = pbox->y1;
    rect->height = (pbox->y2 - pbox->y1);

    rects->mArea += rect->width * rect->height;

    pbox++;
    rect++;
  }

  *aRects = rects;

  return NS_OK;
}

NS_IMETHODIMP
nsRegionXlib::FreeRects(nsRegionRectSet *aRects)
{
  if (nsnull != aRects)
    PR_Free((void *)aRects);

  return NS_OK;
}

NS_IMETHODIMP
nsRegionXlib::GetNativeRegion(void *&aRegion) const
{
  aRegion = (void *)mRegion;
  return NS_OK;
}

NS_IMETHODIMP
nsRegionXlib::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  // cast to avoid const-ness problems on some compilers
  if (((nsRegionXlib*)this)->IsEmpty())
    aComplexity = eRegionComplexity_empty;
  else 
    aComplexity = eRegionComplexity_rect;
      
  return NS_OK;
}

void nsRegionXlib::SetRegionEmpty()
{
  if (!IsEmpty()) { 
    ::XDestroyRegion(mRegion);
  }
}

NS_IMETHODIMP
nsRegionXlib::GetNumRects(PRUint32 *aRects) const
{
  if (!mRegion)
    *aRects = 0;

  *aRects = mRegion->numRects;

  return NS_OK;
}
