/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsRegionQT.h"
#include "prmem.h"
#include "nsRenderingContextQT.h"
#include <qregion.h>

nsRegionQT::nsRegionQT() : mRegion()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::nsRegionQT()\n"));
    NS_INIT_REFCNT();

    mRegionType = eRegionComplexity_empty;
}

nsRegionQT::~nsRegionQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::~nsRegionQT()\n"));
}

NS_IMPL_ISUPPORTS1(nsRegionQT, nsIRegion)
 
nsresult nsRegionQT::Init(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Init()\n"));
    //NS_ADDREF_THIS();
    // should this be here?
    mRegionType = eRegionComplexity_empty;
    return NS_OK;
}

void nsRegionQT::SetTo(const nsIRegion &aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::SetTo()\n"));
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    SetRegionEmpty();
    SetTo(pRegion);
}

void nsRegionQT::SetTo(const nsRegionQT *aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::SetTo()\n"));
    SetRegionEmpty();

    mRegion = aRegion->mRegion;
}

void nsRegionQT::SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::SetTo()\n"));
    SetRegionEmpty();

    QRegion nRegion(aX, aY, aWidth, aHeight);

    mRegion = nRegion;
}

void nsRegionQT::Intersect(const nsIRegion &aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Intersect()\n"));
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    mRegion.intersect(pRegion->mRegion);
}

void nsRegionQT::Intersect(PRInt32 aX, 
                           PRInt32 aY, 
                           PRInt32 aWidth, 
                           PRInt32 aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Intersect()\n"));
    QRegion region(aX, aY, aWidth, aHeight);

    mRegion.intersect(region);
}

void nsRegionQT::Union(const nsIRegion &aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Union()\n"));
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;
 
    mRegion = mRegion.unite(pRegion->mRegion);
}

void nsRegionQT::Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Union()\n"));
    QRegion region(aX, aY, aWidth, aHeight);

    mRegion = mRegion.unite(region);
}

void nsRegionQT::Subtract(const nsIRegion &aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Subtract()\n"));
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    mRegion = mRegion.subtract(pRegion->mRegion);
}

void nsRegionQT::Subtract(PRInt32 aX, 
                          PRInt32 aY, 
                          PRInt32 aWidth, 
                          PRInt32 aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Subtract()\n"));
    QRegion aRegion(aX, aY, aWidth, aHeight);

    mRegion = mRegion.subtract(aRegion);
}

PRBool nsRegionQT::IsEmpty(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::IsEmpty()\n"));
    return mRegion.isEmpty();
}

PRBool nsRegionQT::IsEqual(const nsIRegion &aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::IsEqual()\n"));
    nsRegionQT *pRegion = (nsRegionQT*)&aRegion;

    return (mRegion == pRegion->mRegion);
}

void nsRegionQT::GetBoundingBox(PRInt32 *aX, 
                                PRInt32 *aY, 
                                PRInt32 *aWidth, 
                                PRInt32 *aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::GetBoundingBox()\n"));
    QRect rect = mRegion.boundingRect();

    *aX = rect.x();
    *aY = rect.y();
    *aWidth = rect.width();
    *aHeight = rect.height();
}

void nsRegionQT::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::Offset()\n"));
    mRegion.translate(aXOffset, aYOffset);
}

PRBool nsRegionQT::ContainsRect(PRInt32 aX, 
                                PRInt32 aY, 
                                PRInt32 aWidth, 
                                PRInt32 aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::ContainsRect()\n"));
    QRect rect(aX, aY, aWidth, aHeight);

    return mRegion.contains(rect);
}

NS_IMETHODIMP nsRegionQT::GetRects(nsRegionRectSet **aRects)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::GetRects()\n"));
    NS_ASSERTION(!(nsnull == aRects), "bad ptr");

    QArray<QRect>     array = mRegion.rects();
    PRUint32          size  = array.size();
    nsRegionRect      *rect  = nsnull;
    nsRegionRectSet   *rects = *aRects;

    if (nsnull == rects || rects->mRectsLen < (PRUint32)size) {
        void *buf = PR_Realloc(rects, 
                               sizeof(nsRegionRectSet) + 
                               (sizeof(nsRegionRect) * (size - 1)));

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
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::FreeRects()\n"));
    if (nsnull != aRects) {
        PR_Free((void *)aRects);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRegionQT::GetNativeRegion(void *&aRegion) const
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::GetNativeRegion()\n"));
    aRegion = (void*)&mRegion;
    return NS_OK;
}

NS_IMETHODIMP nsRegionQT::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::GetRegionComplexity()\n"));
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
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRegionQT::SetRegionEmpty()\n"));
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
