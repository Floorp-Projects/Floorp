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
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *	 John C. Griggs <johng@corel.com>
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

#include "nsRegionQt.h"
#include "prmem.h"
#include "nsRenderingContextQt.h"
#include <qregion.h>

#include "qtlog.h"

#ifdef DEBUG
PRUint32 gRegionCount = 0;
PRUint32 gRegionID = 0;
#endif

nsRegionQt::nsRegionQt() : mRegion()
{
#ifdef DEBUG
  gRegionCount++;
  mID = gRegionID++;
  PR_LOG(gQtLogModule, QT_BASIC,
      ("nsRegionQt CTOR (%p) ID: %d, Count: %d\n", this, mID, gRegionCount));
#endif
}

nsRegionQt::~nsRegionQt()
{
#ifdef DEBUG
  gRegionCount--;
  PR_LOG(gQtLogModule, QT_BASIC,
      ("nsRegionQt DTOR (%p) ID: %d, Count: %d\n", this, mID, gRegionCount));
#endif
}

NS_IMPL_ISUPPORTS1(nsRegionQt, nsIRegion)

nsresult nsRegionQt::Init(void)
{
    mRegion = QRegion();
    return NS_OK;
}

void nsRegionQt::SetTo(const nsIRegion &aRegion)
{
    nsRegionQt *pRegion = (nsRegionQt*)&aRegion;

    mRegion = pRegion->mRegion;
}

void nsRegionQt::SetTo(const nsRegionQt *aRegion)
{
    mRegion = aRegion->mRegion;
}

void nsRegionQt::SetTo(PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight)
{
    mRegion = QRegion(aX, aY, aWidth, aHeight);;
}

void nsRegionQt::Intersect(const nsIRegion &aRegion)
{
    nsRegionQt *pRegion = (nsRegionQt*)&aRegion;

    mRegion = mRegion.intersect(pRegion->mRegion);
}

void nsRegionQt::Intersect(PRInt32 aX,PRInt32 aY,
                           PRInt32 aWidth,PRInt32 aHeight)
{
    mRegion = mRegion.intersect(QRect(aX, aY, aWidth, aHeight));
}

void nsRegionQt::Union(const nsIRegion &aRegion)
{
    nsRegionQt *pRegion = (nsRegionQt*)&aRegion;

    mRegion = mRegion.unite(pRegion->mRegion);
}

void nsRegionQt::Union(PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight)
{
    mRegion = mRegion.unite(QRect(aX, aY, aWidth, aHeight));
}

void nsRegionQt::Subtract(const nsIRegion &aRegion)
{
    nsRegionQt *pRegion = (nsRegionQt*)&aRegion;

    mRegion = mRegion.subtract(pRegion->mRegion);
}

void nsRegionQt::Subtract(PRInt32 aX,PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight)
{
    mRegion = mRegion.subtract(QRect(aX, aY, aWidth, aHeight));
}

PRBool nsRegionQt::IsEmpty(void)
{
    return mRegion.isEmpty();
}

PRBool nsRegionQt::IsEqual(const nsIRegion &aRegion)
{
    nsRegionQt *pRegion = (nsRegionQt*)&aRegion;

    return (mRegion == pRegion->mRegion);
}

void nsRegionQt::GetBoundingBox(PRInt32 *aX,PRInt32 *aY,
                                PRInt32 *aWidth,PRInt32 *aHeight)
{
    QRect rect = mRegion.boundingRect();

    *aX = rect.x();
    *aY = rect.y();
    *aWidth = rect.width();
    *aHeight = rect.height();
}

void nsRegionQt::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
    mRegion.translate(aXOffset, aYOffset);
}

PRBool nsRegionQt::ContainsRect(PRInt32 aX,PRInt32 aY,
                                PRInt32 aWidth,PRInt32 aHeight)
{
    return mRegion.contains(QRect(aX, aY, aWidth, aHeight));
}

NS_IMETHODIMP nsRegionQt::GetRects(nsRegionRectSet **aRects)
{
    NS_ASSERTION(!(nsnull == aRects), "bad ptr");

    QMemArray<QRect>   array = mRegion.rects();
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
        const QRect &qRect = array.at(i);

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

NS_IMETHODIMP nsRegionQt::FreeRects(nsRegionRectSet *aRects)
{
    if (nsnull != aRects) {
        PR_Free((void*)aRects);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRegionQt::GetNativeRegion(void *&aRegion) const
{
    aRegion = (void*)&mRegion;
    return NS_OK;
}

NS_IMETHODIMP nsRegionQt::GetRegionComplexity(nsRegionComplexity &aComplexity) const
{
    // cast to avoid const-ness problems on some compilers
    if (mRegion.isEmpty()) {
        aComplexity = eRegionComplexity_empty;
    }
    else {
        aComplexity = eRegionComplexity_rect;
    }
    return NS_OK;
}

void nsRegionQt::SetRegionEmpty()
{
    mRegion = QRegion();
}

NS_IMETHODIMP nsRegionQt::GetNumRects(PRUint32 *aRects) const
{
  *aRects = mRegion.rects().size();

  return NS_OK;
}
