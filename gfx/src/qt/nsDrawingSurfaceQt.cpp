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

#include "nsDrawingSurfaceQt.h"
#include "nsRenderingContextQt.h"
#include <qpaintdevicemetrics.h>

#include "qtlog.h"

#ifdef DEBUG
PRUint32 gDSCount = 0;
PRUint32 gDSID = 0;
#endif

NS_IMPL_ISUPPORTS1(nsDrawingSurfaceQt, nsIDrawingSurface)

nsDrawingSurfaceQt::nsDrawingSurfaceQt()
{
#ifdef DEBUG
    gDSCount++;
    mID = gDSID++;
    PR_LOG(gQtLogModule, QT_BASIC,
           ("nsDrawingSurfaceQt CTOR (%p) ID: %d, Count: %d\n",
            this, mID, gDSCount));
#endif

    mPaintDevice = nsnull;
    mGC          = nsnull;
    mDepth       = -1;
    mWidth       = 0;
    mHeight      = 0;
    mFlags       = 0;
    mLockWidth   = 0;
    mLockHeight  = 0;
    mLockFlags   = 0;
    mLocked      = PR_FALSE;

    // I have no idea how to compute these values.
    // FIXME
    mPixFormat.mRedMask = 0;
    mPixFormat.mGreenMask = 0;
    mPixFormat.mBlueMask = 0;
    mPixFormat.mAlphaMask = 0;

    mPixFormat.mRedShift = 0;
    mPixFormat.mGreenShift = 0;
    mPixFormat.mBlueShift = 0;
    mPixFormat.mAlphaShift = 0;
}

nsDrawingSurfaceQt::~nsDrawingSurfaceQt()
{
#ifdef DEBUG
    gDSCount--;
    PR_LOG(gQtLogModule, QT_BASIC,
           ("nsDrawingSurfaceQt DTOR (%p) ID: %d, Count: %d\n",
            this, mID, gDSCount));
#endif

    if (mGC && mGC->isActive()) {
        mGC->end();
    }

    delete mGC;
    mGC = nsnull;

    if (mPaintDevice) {
        if (mIsOffscreen && !mPaintDevice->paintingActive() && mPaintDevice != &mPixmap)
            delete mPaintDevice;
        mPaintDevice = nsnull;
    }
}

NS_IMETHODIMP nsDrawingSurfaceQt::Lock(PRInt32 aX,PRInt32 aY,
                                       PRUint32 aWidth,PRUint32 aHeight,
                                       void **aBits,PRInt32 *aStride,
                                       PRInt32 *aWidthBytes,PRUint32 aFlags)
{
    if (mLocked) {
        NS_ASSERTION(0, "nested lock attempt");
        return NS_ERROR_FAILURE;
    }
    if (mPixmap.isNull()) {
        NS_ASSERTION(0, "NULL pixmap in lock attempt");
        return NS_ERROR_FAILURE;
    }
    mLocked     = PR_TRUE;
    mLockX      = aX;
    mLockY      = aY;
    mLockWidth  = aWidth;
    mLockHeight = aHeight;
    mLockFlags  = aFlags;

    if (mImage.isNull())
        mImage = mPixmap.convertToImage();

    *aBits = mImage.bits();
    *aStride = mImage.bytesPerLine();
    *aWidthBytes = mImage.bytesPerLine();

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQt::Unlock(void)
{
    if (!mLocked) {
        NS_ASSERTION(0,"attempting to unlock an DrawingSurface that isn't locked");
        return NS_ERROR_FAILURE;
    }
    if (mPixmap.isNull()) {
        NS_ASSERTION(0, "NULL pixmap in unlock attempt");
        return NS_ERROR_FAILURE;
    }
    if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY)) {
        qDebug("nsDrawingSurfaceQt::Unlock: w/h=%d/%d", mPixmap.width(), mPixmap.height());
        mGC->drawPixmap(0, 0, mPixmap, mLockY, mLockY, mLockWidth, mLockHeight);
    }
    mLocked = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQt::GetDimensions(PRUint32 *aWidth,
                                                PRUint32 *aHeight)
{
    *aWidth = mWidth;
    *aHeight = mHeight;

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQt::IsOffscreen(PRBool *aOffScreen)
{
    *aOffScreen = mIsOffscreen;
    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQt::IsPixelAddressable(PRBool *aAddressable)
{
    *aAddressable = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQt::GetPixelFormat(nsPixelFormat *aFormat)
{
    *aFormat = mPixFormat;

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQt::Init(QPaintDevice *aPaintDevice,
                                       QPainter *aGC)
{
    PR_LOG(gQtLogModule, QT_BASIC, ("[%p] nsDrawingSurface::Init\n", this));
    NS_ASSERTION(aPaintDevice, "need paint dev.");
    QPaintDeviceMetrics qMetrics(aPaintDevice);
    mGC = aGC;

    mPaintDevice = aPaintDevice;

    mWidth = qMetrics.width();
    mHeight = qMetrics.height();
    mDepth = qMetrics.depth();

    mIsOffscreen = PR_FALSE;

    mImage.reset();

    return CommonInit();
}

NS_IMETHODIMP nsDrawingSurfaceQt::Init(QPainter *aGC,
                                       PRUint32 aWidth,
                                       PRUint32 aHeight,
                                       PRUint32 aFlags)
{
    PR_LOG(gQtLogModule, QT_BASIC, ("[%p] nsDrawingSurface::Init\n", this));
    if (nsnull == aGC || aWidth <= 0 || aHeight <= 0) {
        return NS_ERROR_FAILURE;
    }
    mGC     = aGC;
    mWidth  = aWidth;
    mHeight = aHeight;
    mFlags  = aFlags;

    mPixmap = QPixmap(mWidth, mHeight, mDepth);
    mPaintDevice = &mPixmap;
    NS_ASSERTION(mPaintDevice, "this better not fail");

    mIsOffscreen = PR_TRUE;

    mImage.reset();

    return CommonInit();
}

NS_IMETHODIMP nsDrawingSurfaceQt::CommonInit()
{
    PR_LOG(gQtLogModule, QT_BASIC, ("[%p] nsDrawingSurface::CommonInit\n", this));
    if (nsnull  == mGC || nsnull == mPaintDevice) {
        return NS_ERROR_FAILURE;
    }
    if (mGC->isActive()) {
        mGC->end();
    }
    mGC->begin(mPaintDevice);
    return NS_OK;
}

QPainter* nsDrawingSurfaceQt::GetGC()
{
    return mGC;
}

QPaintDevice* nsDrawingSurfaceQt::GetPaintDevice()
{
    PR_LOG(gQtLogModule, QT_BASIC, ("[%p] nsDrawingSurfaceQt::GetPaintDevice\n", this));
    NS_ASSERTION(mPaintDevice, "No paint device! Something will probably crash soon.");
    return mPaintDevice;
}
