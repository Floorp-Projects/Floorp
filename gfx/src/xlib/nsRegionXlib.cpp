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

#include "prmem.h"
#include "nsRegionXlib.h"
#include "xregion.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionXlib::nsRegionXlib()
{
  NS_INIT_REFCNT();

  mRegion = nsnull;
  mRegionType = eRegionComplexity_empty;
}

nsRegionXlib::~nsRegionXlib()
{
  if (mRegion)
    ::XDestroyRegion(mRegion);

  mRegion = NULL;
}

NS_IMPL_QUERY_INTERFACE(nsRegionXlib, kRegionIID)
NS_IMPL_ADDREF(nsRegionXlib)
NS_IMPL_RELEASE(nsRegionXlib)

nsresult
nsRegionXlib::Init()
{
  NS_ADDREF_THIS();

  mRegion = ::XCreateRegion();
  mRegionType = eRegionComplexity_empty;

  return NS_OK;
}

void
nsRegionXlib::SetTo(const nsIRegion &aRegion)
{
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;
 
  SetRegionEmpty(); 
 
  Region nRegion = XCreateRegion();
  ::XUnionRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  SetRegionEmpty();

  XRectangle r;

  r.x = aX;
  r.y = aY;
  r.width = aWidth;
  r.height = aHeight;

  Region nRegion = ::XCreateRegion();
  ::XUnionRectWithRegion(&r, mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Intersect(const nsIRegion &aRegion)
{
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;
  
  Region nRegion;
  ::XIntersectRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  Region nRegion;

  ::XIntersectRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(tRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Union(const nsIRegion &aRegion)
{
   nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;
 
   Region nRegion;
   ::XUnionRegion(mRegion, pRegion->mRegion, nRegion);
   ::XDestroyRegion(mRegion);
   mRegion = nRegion;
}

void
nsRegionXlib::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
 
  Region nRegion;
  ::XUnionRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(mRegion);
  ::XDestroyRegion(tRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Subtract(const nsIRegion &aRegion)
{
  nsRegionXlib * pRegion = (nsRegionXlib *)&aRegion;
  
  Region nRegion;
  ::XSubtractRegion(mRegion, pRegion->mRegion, nRegion);
  ::XDestroyRegion(mRegion);
  mRegion = nRegion;
}

void
nsRegionXlib::Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  Region tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  Region nRegion;
  ::XSubtractRegion(mRegion, tRegion, nRegion);
  ::XDestroyRegion(mRegion);
  ::XDestroyRegion(tRegion);
  mRegion = nRegion;
}

PRBool
nsRegionXlib::IsEmpty(void)
{
  return ::XEmptyRegion(mRegion);
}

PRBool
nsRegionXlib::IsEqual(const nsIRegion &aRegion)
{
  nsRegionXlib *pRegion = (nsRegionXlib *)&aRegion;
  
  return ::XEqualRegion(mRegion, pRegion->mRegion);
}

void
nsRegionXlib::GetBoundingBox(PRInt32 *aX, PRInt32 *aY,
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
nsRegionXlib::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  ::XOffsetRegion(mRegion, aXOffset, aYOffset);
}

PRBool
nsRegionXlib::ContainsRect(PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  return ::XRectInRegion(mRegion, aX, aY, aWidth, aHeight);
}

NS_IMETHODIMP
nsRegionXlib::GetRects(nsRegionRectSet **aRects)
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
    mRegion = XCreateRegion();
  }
}

Region
nsRegionXlib::CreateRectRegion(PRInt32 aX,
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
  
  Region rRegion;
  ::XUnionRectWithRegion(&r, tRegion, rRegion);
  ::XDestroyRegion(tRegion);

  return (rRegion);
} 

