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

#include "nsRegionXlib.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionXlib::nsRegionXlib()
{
  NS_INIT_REFCNT();
}

NS_IMPL_QUERY_INTERFACE(nsRegionXlib, kRegionIID)
NS_IMPL_ADDREF(nsRegionXlib)
NS_IMPL_RELEASE(nsRegionXlib)

nsresult
nsRegionXlib::Init()
{
  return 0;
}

void
nsRegionXlib::SetTo(const nsIRegion &aRegion)
{
  return;
}

void
nsRegionXlib::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return;
}

void
nsRegionXlib::Intersect(const nsIRegion &aRegion)
{
  return;
}

void
nsRegionXlib::Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return;
}

void
nsRegionXlib::Union(const nsIRegion &aRegion)
{
  return;
}

void
nsRegionXlib::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return;
}

void
nsRegionXlib::Subtract(const nsIRegion &aRegion)
{
  return;
}

void
nsRegionXlib::Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return;
}

PRBool
nsRegionXlib::IsEmpty(void)
{
  return PR_FALSE;
}

PRBool
nsRegionXlib::IsEqual(const nsIRegion &aRegion)
{
  return PR_FALSE;
}

void
nsRegionXlib::GetBoundingBox(PRInt32 *aX, PRInt32 *aY,
                             PRInt32 *aWidth, PRInt32 *aHeight)
{
  return;
}

void
nsRegionXlib::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  return;
}

PRBool
nsRegionXlib::ContainsRect(PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight)
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsRegionXlib::GetRects(nsRegionRectSet **aRects)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRegionXlib::FreeRects(nsRegionRectSet *aRects)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRegionXlib::GetNativeRegion(void *&aRegion) const
{
  return NS_OK;
}

NS_IMETHODIMP
nsRegionXlib::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
  return NS_OK;
}
