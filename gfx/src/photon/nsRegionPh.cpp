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

#include "nsRegionPh.h"
#include "prmem.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionPh :: nsRegionPh()
{
  NS_INIT_REFCNT();
}

nsRegionPh :: ~nsRegionPh()
{
}

NS_IMPL_QUERY_INTERFACE(nsRegionPh, kRegionIID)
NS_IMPL_ADDREF(nsRegionPh)
NS_IMPL_RELEASE(nsRegionPh)

nsresult nsRegionPh :: Init(void)
{
  return NS_OK;
}

void nsRegionPh :: SetTo(const nsIRegion &aRegion)
{
}

void nsRegionPh :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

void nsRegionPh :: Intersect(const nsIRegion &aRegion)
{
}

void nsRegionPh :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

void nsRegionPh :: Union(const nsIRegion &aRegion)
{
}

void nsRegionPh :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

void nsRegionPh :: Subtract(const nsIRegion &aRegion)
{
}

void nsRegionPh :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

PRBool nsRegionPh :: IsEmpty(void)
{
  return PR_TRUE;
}

PRBool nsRegionPh :: IsEqual(const nsIRegion &aRegion)
{
  return PR_TRUE;
}

void nsRegionPh :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
}

void nsRegionPh :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
}

PRBool nsRegionPh :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return PR_FALSE;
}

NS_IMETHODIMP nsRegionPh :: GetRects(nsRegionRectSet **aRects)
{
  return NS_OK;
}

NS_IMETHODIMP nsRegionPh :: FreeRects(nsRegionRectSet *aRects)
{
  return NS_OK;
}

NS_IMETHODIMP nsRegionPh :: GetNativeRegion(void *&aRegion) const
{
  return NS_OK;
}

NS_IMETHODIMP nsRegionPh :: GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  return NS_OK;
}
