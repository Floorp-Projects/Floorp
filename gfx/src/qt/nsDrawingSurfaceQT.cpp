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

#include "nsDrawingSurfaceQT.h"
#include "nsRenderingContextQT.h"
#include <qpaintdevicemetrics.h>

NS_IMPL_ISUPPORTS2(nsDrawingSurfaceQT, nsIDrawingSurface, nsIDrawingSurfaceQT) 

nsDrawingSurfaceQT::nsDrawingSurfaceQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::nsDrawingSurfaceQT\n"));
    NS_INIT_REFCNT();

    mPaintDevice = nsnull;
    mPixmap      = nsnull;
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

nsDrawingSurfaceQT::~nsDrawingSurfaceQT()
{
    PR_LOG(QtGfxLM,PR_LOG_DEBUG,("nsDrawingSurfaceQT::~nsDrawingSurfaceQT\n"));

    if (mGC && mGC->isActive()) {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDrawingSurfaceQT::~nsDrawingSurfaceQT: calling QPainter::end for %p\n",
                mPaintDevice));
        mGC->end();
    }
    if (mGC) {
      delete mGC;
      mGC = nsnull;
    }
    if (mPixmap) {
      delete mPixmap;
      mPixmap = nsnull;
    }
    if (mPaintDevice) {
      if (mIsOffscreen && !mPaintDevice->paintingActive())
        delete mPaintDevice;
      mPaintDevice = nsnull;
    }
}

NS_IMETHODIMP nsDrawingSurfaceQT::Lock(PRInt32   aX, 
                                       PRInt32   aY,
                                       PRUint32  aWidth, 
                                       PRUint32  aHeight,
                                       void   ** aBits, 
                                       PRInt32 * aStride,
                                       PRInt32 * aWidthBytes, 
                                       PRUint32  aFlags)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::Lock\n"));
    if (mLocked) {
        NS_ASSERTION(0, "nested lock attempt");
        return NS_ERROR_FAILURE;
    }
    if (!mPixmap) {
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
    	mImage = mPixmap->convertToImage();

    *aBits = mImage.bits();
    *aStride = mImage.bytesPerLine();
    *aWidthBytes = mImage.bytesPerLine();

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQT::Unlock(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::Unlock\n"));
    if (!mLocked) {
        NS_ASSERTION(0, "attempting to unlock an DrawingSurface that isn't locked");
        return NS_ERROR_FAILURE;
    }
    if (!mPixmap) {
        NS_ASSERTION(0, "NULL pixmap in unlock attempt");
        return NS_ERROR_FAILURE;
    }
    if (!(mLockFlags & NS_LOCK_SURFACE_READ_ONLY)) {
        mGC->drawPixmap(0, 0, *mPixmap, mLockY, mLockY, mLockWidth, mLockHeight);
    }
    mLocked = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQT::GetDimensions(PRUint32 *aWidth, 
                                                PRUint32 *aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::GetDimensions\n"));
    *aWidth = mWidth;
    *aHeight = mHeight;

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQT::IsOffscreen(PRBool *aOffScreen)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::IsOffscreen\n"));
    *aOffScreen = mIsOffscreen;
    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQT::IsPixelAddressable(PRBool *aAddressable)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::IsPixelAddressable\n"));
    *aAddressable = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQT::GetPixelFormat(nsPixelFormat *aFormat)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::GetPixelFormat\n"));
    *aFormat = mPixFormat;

    return NS_OK;
}

NS_IMETHODIMP nsDrawingSurfaceQT::Init(QPaintDevice *aPaintDevice, 
                                       QPainter *aGC)
{
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsDrawingSurfaceQT::Init: paintdevice=%p, painter=%p\n",
            aPaintDevice, aGC));
    QPaintDeviceMetrics qMetrics(aPaintDevice);
    mGC = aGC;
 
    mPaintDevice = aPaintDevice;

    mWidth = qMetrics.width();
    mHeight = qMetrics.height();
    mDepth = qMetrics.depth();

    mIsOffscreen = PR_FALSE;

    if (!mImage.isNull())
      mImage.reset();

    return CommonInit();
}

NS_IMETHODIMP nsDrawingSurfaceQT::Init(QPainter *aGC, 
                                       PRUint32 aWidth,
                                       PRUint32 aHeight, 
                                       PRUint32 aFlags)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::Init: with no paintdevice\n"));

    if (nsnull == aGC || aWidth <= 0 || aHeight <= 0) {
        return NS_ERROR_FAILURE;
    }
    mGC          = aGC;
    mWidth       = aWidth;
    mHeight      = aHeight;
    mFlags       = aFlags;
 
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsDrawingSurfaceQT::Init: creating pixmap w=%d, h=%d, d=%d\n", 
            mWidth, mHeight, mDepth));
    mPixmap = new QPixmap(mWidth, mHeight, mDepth);
    mPaintDevice = mPixmap;

    mIsOffscreen = PR_TRUE;

    if (!mImage.isNull())
      mImage.reset();

    return CommonInit();
}

NS_IMETHODIMP nsDrawingSurfaceQT::CommonInit()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::CommonInit\n"));

    if (nsnull  == mGC || nsnull == mPaintDevice) {
        return NS_ERROR_FAILURE;
    }
    if (mGC->isActive()) {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDrawingSurfaceQT::CommonInit: calling QPainter::end\n"));
        mGC->end();
    }
    if (mGC->begin(mPaintDevice)) {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDrawingSurfaceQT::CommonInit: QPainter::begin succeeded for %p\n",
                mPaintDevice));
    }
    else {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDrawingSurfaceQT::CommonInit: QPainter::begin failed for %p\n",
                mPaintDevice));
    }
    return NS_OK;
}

QPainter* nsDrawingSurfaceQT::GetGC()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::GetGC\n"));
    return mGC;
}

QPaintDevice* nsDrawingSurfaceQT::GetPaintDevice()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDrawingSurfaceQT::GetPaintDevice\n"));
    return mPaintDevice;
}
