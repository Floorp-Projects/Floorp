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

#include "nsRegionWin.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionWin :: nsRegionWin()
{
  NS_INIT_REFCNT();

  mRegion = NULL;
}

nsRegionWin :: ~nsRegionWin()
{
  mRegion = NULL;
}

NS_IMPL_QUERY_INTERFACE(nsRegionWin, kRegionIID)
NS_IMPL_ADDREF(nsRegionWin)
NS_IMPL_RELEASE(nsRegionWin)

nsresult nsRegionWin :: Init(void)
{
  return NS_OK;
}

void nsRegionWin :: SetTo(const nsIRegion &aRegion)
{
}

void nsRegionWin :: SetTo(const nsRect &aRect)
{
}

void nsRegionWin :: Intersect(const nsIRegion &aRegion)
{
}

void nsRegionWin :: Intersect(const nsRect &aRect)
{
}

void nsRegionWin :: Union(const nsIRegion &aRegion)
{
}

void nsRegionWin :: Union(const nsRect &aRect)
{
}

void nsRegionWin :: Subtract(const nsIRegion &aRegion)
{
}

PRBool nsRegionWin :: IsEmpty(void)
{
  return PR_TRUE;
}

PRBool nsRegionWin :: IsEqual(const nsIRegion &aRegion)
{
  return PR_FALSE;
}

void nsRegionWin :: GetBoundingBox(nsRect &aRect)
{
}

void nsRegionWin :: Offset(nscoord aXOffset, nscoord aYOffset)
{
}

PRBool nsRegionWin :: ContainsRect(const nsRect &aRect)
{
  return PR_FALSE;
}

PRBool nsRegionWin :: ForEachRect(nsRectInRegionFunc *func, void *closure)
{
  return PR_FALSE;
}

HRGN nsRegionWin :: GetHRGN(void)
{
  return mRegion;
}
