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

#include <gtk/gtk.h>
#include "nsRegionGTK.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionGTK::nsRegionGTK()
{
  NS_INIT_REFCNT();
  
  mRegion = ::gdk_region_new();
  mRegionType = eRegionComplexity_empty;
}

nsRegionGTK::~nsRegionGTK()
{
   if (mRegion)
     ::gdk_region_destroy(mRegion);
  mRegion = nsnull;
}


NS_IMPL_QUERY_INTERFACE(nsRegionGTK, kRegionIID)
NS_IMPL_ADDREF(nsRegionGTK)
NS_IMPL_RELEASE(nsRegionGTK)

nsresult nsRegionGTK::Init(void)
{
  if (mRegion)
    ::gdk_region_destroy(mRegion);
  mRegion = ::gdk_region_new();  //correct?
  mRegionType = eRegionComplexity_empty;
  
  return NS_OK;
}

void nsRegionGTK::SetTo(const nsIRegion &aRegion)
{
  nsRegionGTK * pRegion = (nsRegionGTK *)&aRegion;

  SetRegionEmpty();
  
  GdkRegion *nRegion = ::gdk_regions_union(mRegion, pRegion->mRegion);
  ::gdk_region_destroy(mRegion);
  mRegion = nRegion;
}

void nsRegionGTK::SetTo(const nsRegionGTK *aRegion)
{
  SetRegionEmpty();
  
  GdkRegion *nRegion = ::gdk_regions_union(mRegion, aRegion->mRegion);
  ::gdk_region_destroy(mRegion);
  mRegion = nRegion;
}

void nsRegionGTK::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

  SetRegionEmpty();

  GdkRectangle grect;

  grect.x = aX;
  grect.y = aY;
  grect.width = aWidth;
  grect.height = aHeight;
  
  GdkRegion *nRegion = ::gdk_region_union_with_rect(mRegion, &grect);
  ::gdk_region_destroy(mRegion);
  mRegion = nRegion;
}

void nsRegionGTK::Intersect(const nsIRegion &aRegion)
{
  nsRegionGTK * pRegion = (nsRegionGTK *)&aRegion;

  GdkRegion *nRegion = ::gdk_regions_intersect(mRegion, pRegion->mRegion);
  ::gdk_region_destroy(mRegion);
  mRegion = nRegion;
}

void nsRegionGTK::Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  GdkRegion *tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  GdkRegion *nRegion = ::gdk_regions_intersect(mRegion, tRegion);
   ::gdk_region_destroy(tRegion);
   ::gdk_region_destroy(mRegion);
   mRegion = nRegion;

}

void nsRegionGTK::Union(const nsIRegion &aRegion)
{
   nsRegionGTK * pRegion = (nsRegionGTK *)&aRegion;
   
   GdkRegion *nRegion = ::gdk_regions_union(mRegion, pRegion->mRegion);
   ::gdk_region_destroy(mRegion);
   mRegion = nRegion;
}

void nsRegionGTK::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{

  GdkRegion *tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);

  GdkRegion *nRegion = ::gdk_regions_union(mRegion, tRegion);
  ::gdk_region_destroy(mRegion);
  ::gdk_region_destroy(tRegion);
  mRegion = nRegion;
}

void nsRegionGTK::Subtract(const nsIRegion &aRegion)
{
  nsRegionGTK * pRegion = (nsRegionGTK *)&aRegion;

   GdkRegion *nRegion = ::gdk_regions_subtract(mRegion, pRegion->mRegion);
   ::gdk_region_destroy(mRegion);
   mRegion = nRegion;
}

void nsRegionGTK::Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  GdkRegion *tRegion = CreateRectRegion(aX, aY, aWidth, aHeight);
  
  GdkRegion *nRegion = ::gdk_regions_subtract(mRegion, tRegion);
  ::gdk_region_destroy(mRegion);
  ::gdk_region_destroy(tRegion);
  mRegion = nRegion;
}

PRBool nsRegionGTK::IsEmpty(void)
{
  return (::gdk_region_empty(mRegion));
}

PRBool nsRegionGTK::IsEqual(const nsIRegion &aRegion)
{
  nsRegionGTK *pRegion = (nsRegionGTK *)&aRegion;

  return(::gdk_region_equal(mRegion, pRegion->mRegion));

}

void nsRegionGTK::GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  GdkRectangle rect;

  ::gdk_region_get_clipbox(mRegion, &rect);

  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;
}

void nsRegionGTK::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
   ::gdk_region_offset(mRegion, aXOffset, aYOffset);
}

PRBool nsRegionGTK::ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  GdkOverlapType containment;
  GdkRectangle rect;
   
   rect.x=aX;
   rect.y = aY;
   rect.width = aWidth;
   rect.height = aHeight;
   
  containment = ::gdk_region_rect_in(mRegion, &rect);

  if (containment == GDK_OVERLAP_RECTANGLE_IN)
    return PR_TRUE;
  else
    return PR_FALSE;

}

PRBool nsRegionGTK::ForEachRect(nsRectInRegionFunc *func, void *closure)
{
  return PR_FALSE;
}

NS_IMETHODIMP nsRegionGTK::GetNativeRegion(void *&aRegion) const
{
  aRegion = (void *)mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsRegionGTK::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  if (IsEmpty())
    aComplexity = eRegionComplexity_empty;
  else
    aComplexity = eRegionComplexity_rect;

  return NS_OK;
}

void nsRegionGTK::SetRegionEmpty()
{
  ::gdk_region_destroy(mRegion);
  mRegion = ::gdk_region_new();
}

GdkRegion *nsRegionGTK::CreateRectRegion(PRInt32 aX,
					 PRInt32 aY,
					 PRInt32 aWidth,
					 PRInt32 aHeight)
{
  GdkRegion *tRegion = ::gdk_region_new();
  GdkRectangle rect;

  rect.x = aX;
  rect.y = aY;
  rect.width = aWidth;
  rect.height = aHeight;

  GdkRegion *rRegion = ::gdk_region_union_with_rect(tRegion, &rect);
  ::gdk_region_destroy(tRegion);
  
  return (rRegion);
}
