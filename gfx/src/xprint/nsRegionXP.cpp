/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "prmem.h"
#include "nsRegionXP.h"
#include "xregion.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionXP::nsRegionXP()
{
  NS_INIT_REFCNT();

  mRegion = nsnull;
  mRegionType = eRegionComplexity_empty;
}

nsRegionXP::~nsRegionXP()
{
  if (mRegion)
    ::XDestroyRegion(mRegion);

  mRegion = NULL;
}

NS_IMPL_QUERY_INTERFACE(nsRegionXP, kRegionIID)
NS_IMPL_ADDREF(nsRegionXP)
NS_IMPL_RELEASE(nsRegionXP)

nsresult
nsRegionXP::Init()
{
  NS_ADDREF_THIS();

  mRegion = ::XCreateRegion();
  mRegionType = eRegionComplexity_empty;

  return NS_OK;
}

void
nsRegionXP::SetTo(const nsIRegion &aRegion)
{
  nsRegionXP * pRegion = (nsRegionXP *)&aRegion;
 
  SetRegionEmpty(); 
 
  Region nRegion = XCreateRegion();
  ::XUnionRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXP::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  SetRegionEmpty();

  XRectangle r;

  r.x = aX;
  r.y = aY;
  r.width = aWidth;
  r.height = aHeight;

  Region nRegion = ::XCreateRegion();
  Region tRegion = ::XCreateRegion();
  ::XUnionRectWithRegion(&r, tRegion, nRegion);
  ::XDestroyRegion(mRegion);
  ::XDestroyRegion(tRegion);
  mRegion = nRegion;
}

void
nsRegionXP::Intersect(const nsIRegion &aRegion)
{
  nsRegionXP * pRegion = (nsRegionXP *)&aRegion;
  
  Region nRegion = XCreateRegion();
  ::XIntersectRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXP::Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  Region nRegion = XCreateRegion();

  ::XIntersectRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(tRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXP::Union(const nsIRegion &aRegion)
{
   nsRegionXP * pRegion = (nsRegionXP *)&aRegion;
 
   Region nRegion = XCreateRegion();
   ::XUnionRegion(mRegion, pRegion->mRegion, nRegion);
   ::XDestroyRegion(mRegion);
   mRegion = nRegion;
}

void
nsRegionXP::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
 
  Region nRegion = XCreateRegion();
  ::XUnionRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(mRegion);
  ::XDestroyRegion(tRegion);
  mRegion = nRegion;
}

void
nsRegionXP::Subtract(const nsIRegion &aRegion)
{
  nsRegionXP * pRegion = (nsRegionXP *)&aRegion;
  
  Region nRegion = XCreateRegion();
  ::XSubtractRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXP::Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  Region nRegion = XCreateRegion();
  ::XSubtractRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(mRegion);
  ::XDestroyRegion(tRegion);
  mRegion = nRegion;
}

PRBool
nsRegionXP::IsEmpty(void)
{
  return ::XEmptyRegion(mRegion);
}

PRBool
nsRegionXP::IsEqual(const nsIRegion &aRegion)
{
  nsRegionXP *pRegion = (nsRegionXP *)&aRegion;
  
  return ::XEqualRegion(mRegion, pRegion->mRegion);
}

void
nsRegionXP::GetBoundingBox(PRInt32 *aX, PRInt32 *aY,
                             PRInt32 *aWidth, PRInt32 *aHeight)
{
  XRectangle r;

  ::XClipBox(mRegion, &r);

  *aX = r.x;
  *aY = r.y;
  *aWidth = r.width;
  *aHeight = r.height;
}

void
nsRegionXP::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  ::XOffsetRegion(mRegion, aXOffset, aYOffset);
}

PRBool
nsRegionXP::ContainsRect(PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  return ::XRectInRegion(mRegion, aX, aY, aWidth, aHeight);
}

NS_IMETHODIMP
nsRegionXP::GetRects(nsRegionRectSet **aRects)
{
  nsRegionRectSet   *rects;
  int               nbox;
  BOX               *pbox;  
  nsRegionRect      *rect;
  
  NS_ASSERTION(!(nsnull == aRects), "bad ptr");
 
  //code lifted from old xfe. MMP
     
  pbox = mRegion->rects;
  nbox = mRegion->numRects;
 
  rects = *aRects;

  if ((nsnull == rects) || (rects->mRectsLen < (PRUint32)nbox))
  {
    void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) + (sizeof(nsRegionRect
) * (nbox - 1)));
    
    if (nsnull == buf)
    {
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

  while (nbox--)
  {
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
nsRegionXP::FreeRects(nsRegionRectSet *aRects)
{
  if (nsnull != aRects)
    PR_Free((void *)aRects);

  return NS_OK;
}

NS_IMETHODIMP
nsRegionXP::GetNativeRegion(void *&aRegion) const
{
  aRegion = (void *)mRegion;
  return NS_OK;
}

NS_IMETHODIMP
nsRegionXP::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  // cast to avoid const-ness problems on some compilers
  if (((nsRegionXP*)this)->IsEmpty())
    aComplexity = eRegionComplexity_empty;
  else 
    aComplexity = eRegionComplexity_rect;
      
  return NS_OK;
}

void nsRegionXP::SetRegionEmpty()
{ 
  if (!IsEmpty()) { 
    ::XDestroyRegion(mRegion);
    mRegion = XCreateRegion();
  }
}

Region
nsRegionXP::CreateRectRegion(PRInt32 aX,
                               PRInt32 aY,
                               PRInt32 aWidth,
                               PRInt32 aHeight)
{
  Region tRegion = XCreateRegion();
  XRectangle r;

  r.x = aX;
  r.y = aY;
  r.width = aWidth;
  r.height = aHeight;
  
  ::XUnionRectWithRegion(&r, tRegion, tRegion);

  return (tRegion);
} 

NS_IMETHODIMP nsRegionXP::GetNumRects(PRUint32 *aRects) const
{

  *aRects = mRegion->numRects;

  return NS_OK;
}

