/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *		John C. Griggs <johng@corel.com>
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

#include "nsRegionQT.h"
#include "prmem.h"
#include "nsRenderingContextQT.h"
#include <qregion.h>

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gRegionCount = 0;
PRUint32 gRegionID = 0;
#endif

nsRegionQT::nsRegionQT() : mRegion()
{
#ifdef DBG_JCG
  gRegionCount++;
  mID = gRegionID++;
  printf("JCG: nsRegionQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gRegionCount);
#endif
  NS_INIT_REFCNT();
}

nsRegionQT::~nsRegionQT()
{
#ifdef DBG_JCG
  gRegionCount--;
  printf("JCG: nsRegionQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gRegionCount);
#endif
}

NS_IMPL_ISUPPORTS1(nsRegionQT, nsIRegion)
 
nsresult nsRegionQT::Init(void)
{
    SetRegionEmpty();
    return NS_OK;
}

void nsRegionQT::SetTo(const nsIRegion &aRegion)
{
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    SetRegionEmpty();
    SetTo(pRegion);
}

void nsRegionQT::SetTo(const nsRegionQT *aRegion)
{
    SetRegionEmpty();

    mRegion = aRegion->mRegion;
}

void nsRegionQT::SetTo(PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight)
{
    SetRegionEmpty();

    QRegion nRegion(aX, aY, aWidth, aHeight);

    mRegion = nRegion;
}

void nsRegionQT::Intersect(const nsIRegion &aRegion)
{
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    mRegion = mRegion.intersect(pRegion->mRegion);
}

void nsRegionQT::Intersect(PRInt32 aX,PRInt32 aY,
                           PRInt32 aWidth,PRInt32 aHeight)
{
    QRegion region(aX, aY, aWidth, aHeight);

    mRegion = mRegion.intersect(region);
}

void nsRegionQT::Union(const nsIRegion &aRegion)
{
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;
 
    mRegion = mRegion.unite(pRegion->mRegion);
}

void nsRegionQT::Union(PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight)
{
    QRegion region(aX, aY, aWidth, aHeight);

    mRegion = mRegion.unite(region);
}

void nsRegionQT::Subtract(const nsIRegion &aRegion)
{
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    mRegion = mRegion.subtract(pRegion->mRegion);
}

void nsRegionQT::Subtract(PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight)
{
    QRegion aRegion(aX, aY, aWidth, aHeight);

    mRegion = mRegion.subtract(aRegion);
}

PRBool nsRegionQT::IsEmpty(void)
{
    return mRegion.isEmpty();
}

PRBool nsRegionQT::IsEqual(const nsIRegion &aRegion)
{
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    return (mRegion == pRegion->mRegion);
}

void nsRegionQT::GetBoundingBox(PRInt32 *aX,PRInt32 *aY,
                                PRInt32 *aWidth,PRInt32 *aHeight)
{
    QRect rect = mRegion.boundingRect();

    *aX = rect.x();
    *aY = rect.y();
    *aWidth = rect.width();
    *aHeight = rect.height();
}

void nsRegionQT::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
    mRegion.translate(aXOffset, aYOffset);
}

PRBool nsRegionQT::ContainsRect(PRInt32 aX,PRInt32 aY,
                                PRInt32 aWidth,PRInt32 aHeight)
{
    QRect rect(aX, aY, aWidth, aHeight);

    return mRegion.contains(rect);
}

NS_IMETHODIMP nsRegionQT::GetRects(nsRegionRectSet **aRects)
{
    NS_ASSERTION(!(nsnull == aRects), "bad ptr");

    QArray<QRect>   array = mRegion.rects();
    PRUint32        size  = array.size();
    nsRegionRect    *rect  = nsnull;
    nsRegionRectSet *rects = *aRects;

    if (nsnull == rects || rects->mRectsLen < (PRUint32)size) {
        void *buf = PR_Realloc(rects, 
                               sizeof(nsRegionRectSet)
                                + (sizeof(nsRegionRect) * (size - 1)));

        if (nsnull == buf) {
            if (nsnull != rects)
                rects->mNumRects = 0;
            return NS_OK;
        }
        rects = (nsRegionRectSet*)buf;
        rects->mRectsLen = size;
    }
    rects->mNumRects = size;
    rects->mArea = 0;
    rect = &rects->mRects[0];

    for (PRUint32 i = 0; i < size; i++) {
        QRect qRect = array[i];

        rect->x = qRect.x();
        rect->y = qRect.y();
        rect->width = qRect.width();
        rect->height = qRect.height();

        rects->mArea += rect->width * rect->height;

        rect++;
    }
    *aRects = rects;
    return NS_OK;        
}

NS_IMETHODIMP nsRegionQT::FreeRects(nsRegionRectSet *aRects)
{
    if (nsnull != aRects) {
        PR_Free((void*)aRects);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRegionQT::GetNativeRegion(void *&aRegion) const
{
    aRegion = (void*)&mRegion;
    return NS_OK;
}

NS_IMETHODIMP nsRegionQT::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
    // cast to avoid const-ness problems on some compilers
    if (((nsRegionQT*)this)->IsEmpty()) {
        aComplexity = eRegionComplexity_empty;
    }
    else {
        aComplexity = eRegionComplexity_rect;
    }
    return NS_OK;
}

void nsRegionQT::SetRegionEmpty()
{
    if (!IsEmpty()) {
        QRegion empty;
        mRegion = empty;
    }
}

NS_IMETHODIMP nsRegionQT::GetNumRects(PRUint32 *aRects) const
{
  *aRects = mRegion.rects().size();
 
  return NS_OK;
}
