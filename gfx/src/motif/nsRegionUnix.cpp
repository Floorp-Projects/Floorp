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
}

nsRegionUnix :: ~nsRegionUnix()
{
}

NS_IMPL_QUERY_INTERFACE(nsRegionUnix, kRegionIID)
NS_IMPL_ADDREF(nsRegionUnix)
NS_IMPL_RELEASE(nsRegionUnix)

nsresult nsRegionUnix :: Init(void)
{
  return NS_OK;
}

void nsRegionUnix :: SetTo(const nsIRegion &aRegion)
{
}

void nsRegionUnix :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

void nsRegionUnix :: Intersect(const nsIRegion &aRegion)
{
}

void nsRegionUnix :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

void nsRegionUnix :: Union(const nsIRegion &aRegion)
{
}

void nsRegionUnix :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
}

void nsRegionUnix :: Subtract(const nsIRegion &aRegion)
{
}

PRBool nsRegionUnix :: IsEmpty(void)
{
  return PR_FALSE;
}

PRBool nsRegionUnix :: IsEqual(const nsIRegion &aRegion)
{
  return PR_FALSE;
}

void nsRegionUnix :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aX = *aY = *aWidth = *aHeight = 0;
}

void nsRegionUnix :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
}

PRBool nsRegionUnix :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return PR_TRUE;
}

PRBool nsRegionUnix :: ForEachRect(nsRectInRegionFunc *func, void *closure)
{
  return PR_FALSE;
}
