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

#include <Region.h>

#include "nsRegionBeOS.h"
#include "prmem.h"

static NS_DEFINE_IID(kRegionIID, NS_IREGION_IID);

nsRegionBeOS :: nsRegionBeOS()
{
  NS_INIT_REFCNT();

  mRegion.MakeEmpty();
  mRegionType = eRegionComplexity_empty;
}

nsRegionBeOS :: ~nsRegionBeOS()
{
  mRegion.MakeEmpty();
}

NS_IMPL_QUERY_INTERFACE(nsRegionBeOS, kRegionIID)
NS_IMPL_ADDREF(nsRegionBeOS)
NS_IMPL_RELEASE(nsRegionBeOS)

nsresult nsRegionBeOS :: Init(void)
{
  mRegion.MakeEmpty();
  mRegionType = eRegionComplexity_empty;
  return NS_OK;
}

void nsRegionBeOS :: SetTo(const nsIRegion &aRegion)
{
  nsRegionBeOS *pRegion = (nsRegionBeOS *)&aRegion;

  mRegion = pRegion->mRegion;
  SetRegionType();
}

void nsRegionBeOS :: SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion.Set(BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
	SetRegionType();
}

void nsRegionBeOS :: Intersect(const nsIRegion &aRegion)
{
	nsRegionBeOS *pRegion = (nsRegionBeOS*)&aRegion;

	mRegion.IntersectWith(&pRegion->mRegion);
	SetRegionType();
}

void nsRegionBeOS :: Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  BRegion tRegion;
  tRegion.Set( BRect( aX, aY, aX + aWidth - 1, aY + aHeight - 1 ) );
  mRegion.IntersectWith(&tRegion);
  SetRegionType();
}

void nsRegionBeOS :: Union(const nsIRegion &aRegion)
{
	nsRegionBeOS *pRegion = (nsRegionBeOS*)&aRegion;

	mRegion.Include(&pRegion->mRegion);
	SetRegionType();
}

void nsRegionBeOS :: Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion.Include(BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
	SetRegionType();
}

void nsRegionBeOS :: Subtract(const nsIRegion &aRegion)
{
  nsRegionBeOS *pRegion = (nsRegionBeOS*)&aRegion;

  mRegion.Exclude(&pRegion->mRegion);
  SetRegionType();
}

void nsRegionBeOS :: Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion.Exclude(BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
	SetRegionType();
}

PRBool nsRegionBeOS :: IsEmpty(void)
{
  if( mRegionType == eRegionComplexity_empty )
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsRegionBeOS :: IsEqual(const nsIRegion &aRegion)
{
  printf(" - nsRegionBeOS :: IsEqual not implemented!\n");
  return PR_FALSE;
}

void nsRegionBeOS :: GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  if( mRegion.CountRects() ) {
    BRect rect = mRegion.Frame();
    *aX = nscoord(rect.left);
    *aY = nscoord(rect.top);
    *aWidth = nscoord(rect.Width());
    *aHeight = nscoord(rect.Height());
  }
  else
  {
  	*aX = *aY = *aWidth = *aHeight = 0;
  }
}

void nsRegionBeOS :: Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
	mRegion.OffsetBy( aXOffset, aYOffset );
}

void nsRegionBeOS :: SetRegionType(void)
{
  if(mRegion.CountRects() == 1)
    mRegionType = eRegionComplexity_rect ;
  else if(mRegion.CountRects() > 1)
    mRegionType = eRegionComplexity_complex ;
  else
    mRegionType = eRegionComplexity_empty;
}

PRBool nsRegionBeOS :: ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	return mRegion.Intersects(BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
}

NS_IMETHODIMP nsRegionBeOS :: GetRects(nsRegionRectSet **aRects)
{
	nsRegionRectSet   *rects;
	int               nbox;
	nsRegionRect      *rect;
	
	NS_ASSERTION(!(nsnull == aRects), "bad ptr");
	
	//code lifted from old xfe. MMP
	
	nbox = mRegion.CountRects();
	
	rects = *aRects;
	
	if ((nsnull == rects) || (rects->mRectsLen < (PRUint32)nbox))
	{
		void *buf = PR_Realloc(rects, sizeof(nsRegionRectSet) + (sizeof(nsRegionRect) * (nbox - 1)));

		if(nsnull == buf)
		{
			if(nsnull != rects)
				rects->mNumRects = 0;

			return NS_OK;
		}

		rects = (nsRegionRectSet *)buf;
		rects->mRectsLen = nbox;
	}

	rects->mNumRects = nbox;
	rects->mArea = 0;
	rect = &rects->mRects[0];

	for(int32 i = 0; i < nbox; i++)
	{
		BRect r = mRegion.RectAt(i);
		rect->x = r.left;
		rect->width = (r.right - r.left + 1);
		rect->y = r.top;
		rect->height = (r.bottom - r.top + 1);

		rects->mArea += rect->width * rect->height;

		rect++;
	}

	*aRects = rects;

	return NS_OK;
}

NS_IMETHODIMP nsRegionBeOS :: FreeRects(nsRegionRectSet *aRects)
{
	if(nsnull != aRects)
		PR_Free((void *)aRects);

	return NS_OK;
}

NS_IMETHODIMP nsRegionBeOS :: GetNativeRegion(void *&aRegion) const
{
	aRegion = (void *)&mRegion;
	return NS_OK;
}

NS_IMETHODIMP nsRegionBeOS :: GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
	aComplexity = mRegionType;
	return NS_OK;
}
