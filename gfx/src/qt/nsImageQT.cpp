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

#include "nsImageQT.h"
#include "nsRenderingContextQT.h"

#include "nspr.h"

#include <qpixmap.h>
#include <qimage.h>

#define IsFlagSet(a,b) ((a) & (b))

#undef CHEAP_PERFORMANCE_MEASUREMENT

NS_IMPL_ISUPPORTS1(nsImageQT, nsIImage)
 
//------------------------------------------------------------

nsImageQT::nsImageQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::nsImageQT()\n"));
    NS_INIT_REFCNT();
    mImageBits   = nsnull;
    mWidth       = 0;
    mHeight      = 0;
    mDepth       = 0;
    mRequestDepth = 0;
    mAlphaBits   = nsnull;
    mAlphaPixmap = nsnull;
    mImagePixmap = nsnull;
    mAlphaDepth = 0;
    mRowBytes = 0;
    mSizeImage = 0;
    mAlphaHeight = 0;
    mAlphaWidth = 0;
    mAlphaRowBytes = 0;
    mNumBytesPixel = 0;
    mIsTopToBottom = PR_TRUE;
    mDecodedX1 = 0; 
    mDecodedY1 = 0; 
    mDecodedX2 = 0; 
    mDecodedY2 = 0; 
    mNaturalWidth = 0;
    mNaturalHeight = 0;
}

//------------------------------------------------------------

nsImageQT::~nsImageQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::~nsImageQT()\n"));
    if (nsnull != mImageBits) {
        delete[] (PRUint8*)mImageBits;
        mImageBits = nsnull;
    }
    if (nsnull != mAlphaBits) {
        delete[] (PRUint8*)mAlphaBits;
        mAlphaBits = nsnull;
    }
    if (nsnull != mAlphaPixmap) {
        delete mAlphaPixmap;
    }
    if (nsnull != mImagePixmap) {
        delete mImagePixmap;
    }
}

//------------------------------------------------------------

nsresult nsImageQT::Init(PRInt32 aWidth, 
                         PRInt32 aHeight,
                         PRInt32 aDepth, 
                         nsMaskRequirements aMaskRequirements)
{
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Init: width=%d height=%d depth=%d\n", 
            aWidth, 
            aHeight, 
            aDepth));
    if (aWidth == 0 && aHeight == 0) {
        return NS_ERROR_FAILURE;
    }
    if (nsnull != mImageBits) {
        delete[] (PRUint8*)mImageBits;
        mImageBits = nsnull;
    }
    if (nsnull != mAlphaBits) {
        delete[] (PRUint8*)mAlphaBits;
        mAlphaBits = nsnull;
    }
    if (nsnull != mAlphaPixmap) {
        delete mAlphaPixmap;
        mAlphaPixmap = nsnull;
    }
    SetDecodedRect(0,0,0,0);  //init
    SetNaturalWidth(0);
    SetNaturalHeight(0);

    // mImagePixmap gets created once per unique image bits in Draw()
    // ImageUpdated(nsImageUpdateFlags_kBitsChanged) can cause the
    // image bits to change and mImagePixmap will be unrefed and nulled.
    if (nsnull != mImagePixmap) {
        delete mImagePixmap;
        mImagePixmap = nsnull;
    }
    if (32 == aDepth) {
        mNumBytesPixel = 4;
        mDepth = aDepth;
        mRequestDepth = mDepth;
    }
    else if (24 == aDepth) {
        mNumBytesPixel = 4;
        mDepth = 32;
        mRequestDepth = aDepth;
    }
    else 
    {
        NS_ASSERTION(PR_FALSE, "unexpected image depth");
        return NS_ERROR_UNEXPECTED;
    }
    mWidth = aWidth;
    mHeight = aHeight;
    mIsTopToBottom = PR_TRUE;

    // create the memory for the image
    ComputeMetrics();

    mImageBits = (PRUint8*)new PRUint8[mSizeImage];

    switch(aMaskRequirements)
    {
      case nsMaskRequirements_kNoMask:
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Init: no mask needed\n"));
        mAlphaBits = nsnull;
        mAlphaWidth = 0;
        mAlphaHeight = 0;
        break;

      case nsMaskRequirements_kNeeds1Bit:
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Init: 1 bit mask needed\n"));
        mAlphaRowBytes = (aWidth + 7) / 8;
        mAlphaDepth = 1;

        // 32-bit align each row
        mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

        mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
        mAlphaWidth = aWidth;
        mAlphaHeight = aHeight;
        break;

      case nsMaskRequirements_kNeeds8Bit:
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Init: 8 bit mask needed\n"));
        mAlphaRowBytes = aWidth;
        mAlphaDepth = 8;
 
        // 32-bit align each row
        mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
        mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight];
        mAlphaWidth = aWidth;
        mAlphaHeight = aHeight;
        break;
    }
    return NS_OK;
}

//------------------------------------------------------------

void nsImageQT::ComputeMetrics()
{
    mRowBytes = CalcBytesSpan(mWidth);
    mSizeImage = mRowBytes * mHeight;
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::ComputeMetrics: mRowBytes=%d, mSizeImage=%d\n", 
            mRowBytes,
            mSizeImage));
}

PRInt32 nsImageQT::GetHeight()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetHeight()\n"));
    return mHeight;
}

PRInt32 nsImageQT::GetWidth()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetWidth()\n"));
    return mWidth;
}

PRUint8 * nsImageQT::GetBits()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetBits()\n"));
    return mImageBits;
}

void * nsImageQT::GetBitInfo()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetBitInfo()\n"));
    return nsnull;
}

PRInt32 nsImageQT::GetLineStride()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetLineStride()\n"));
    return mRowBytes;
}

nsColorMap * nsImageQT::GetColorMap()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetColorMap()\n"));
    return nsnull;
}

PRBool nsImageQT::IsOptimized()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::IsOptimized()\n"));
    return PR_TRUE;
}

PRUint8 * nsImageQT::GetAlphaBits()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetAlphaBits()\n"));
    return mAlphaBits;
}

PRInt32 nsImageQT::GetAlphaWidth()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetAlphaWidth()\n"));
    return mAlphaWidth;
}

PRInt32 nsImageQT::GetAlphaHeight()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetAlphaHeight()\n"));
    return mAlphaHeight;
}

PRInt32 nsImageQT::GetAlphaLineStride()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetAlphaLineStride()\n"));
    return mAlphaRowBytes;
}

nsIImage * nsImageQT::DuplicateImage()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::DuplicateImage()\n"));
    return nsnull;
}

void nsImageQT::SetAlphaLevel(PRInt32 aAlphaLevel)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::SetAlphaLevel()\n"));
}

PRInt32 nsImageQT::GetAlphaLevel()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetAlphaLevel()\n"));
    return 0;
}

void nsImageQT::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::MoveAlphaMask()\n"));
}

//------------------------------------------------------------

PRInt32 nsImageQT::CalcBytesSpan(PRUint32  aWidth)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::CalcBytesSpan()\n"));
    PRInt32 spanbytes;

    spanbytes = (aWidth * mDepth) >> 5;

    if (((PRUint32)aWidth * mDepth) & 0x1F)
    {
        spanbytes++;
    }
    spanbytes <<= 2;
    return(spanbytes);
}

//------------------------------------------------------------

// set up the palette to the passed in color array, RGB only in this array
void nsImageQT::ImageUpdated(nsIDeviceContext *aContext,
                             PRUint8 aFlags,
                             nsRect *aUpdateRect)
{
    if (IsFlagSet(nsImageUpdateFlags_kBitsChanged, aFlags))
    {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsImageQT::ImageUpdated:Update flags changed\n"));
        if (nsnull != mAlphaPixmap) 
        {
            delete mAlphaPixmap;
            mAlphaPixmap = nsnull;
        }

        // mImagePixmap gets created once per unique image bits in Draw()
        // ImageUpdated(nsImageUpdateFlags_kBitsChanged) can cause the
        // image bits to change and mImagePixmap will be unrefed and nulled.
        if (nsnull != mImagePixmap) 
        {
            delete mImagePixmap;
            mImagePixmap = nsnull;
        }
    }
}

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP nsImageQT::Draw(nsIRenderingContext &aContext, 
                              nsDrawingSurface aSurface,
                              PRInt32 aSX, 
                              PRInt32 aSY, 
                              PRInt32 aSWidth, 
                              PRInt32 aSHeight,
                              PRInt32 aDX, 
                              PRInt32 aDY, 
                              PRInt32 aDWidth, 
                              PRInt32 aDHeight)
{
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw with source and destination coordinates\n"));
    if (nsnull == aSurface) {
        return NS_ERROR_FAILURE;
    }
    // Render unique image bits onto an off screen pixmap only once
    // The image bits can change as a result of ImageUpdated() - for
    // example: animated GIFs.
    if (nsnull == mImagePixmap) {
      CreateImagePixmap(aSWidth,aSHeight);
    }
    if (nsnull == mImagePixmap)
        return NS_ERROR_FAILURE;

    // Copy data from mImageBits to the drawing surface specified by aSurface.
    // We only want a subset of the data in mImageBits, and the data might
    // need to be scaled.
    nsDrawingSurfaceQT *drawing = (nsDrawingSurfaceQT*)aSurface;

    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw: destx=%d, desty=%d, destwidth=%d, destheight=%d\n",
            aDX,
            aDY,
            aDWidth,
            aDHeight));
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw: srcx=%d, srcy=%d, srcwidth=%d, srcheight=%d\n",
            aSX,
            aSY,
            aSWidth,
            aSHeight));

    drawing->GetGC()->drawImage(aDX, 
                                aDY, 
                                *mImagePixmap, 
                                aSX, 
                                aSY, 
                                aSWidth, 
                                aSHeight);
    return NS_OK;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP nsImageQT::Draw(nsIRenderingContext &aContext,
                              nsDrawingSurface aSurface,
                              PRInt32 aX, 
                              PRInt32 aY,
                              PRInt32 aWidth, 
                              PRInt32 aHeight)
{
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw with just destination coordinates\n"));
    if (nsnull == aSurface) {
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Draw: no surface!!\n"));
        return NS_ERROR_FAILURE;
    }

    // XXX kipp: this is temporary code until we eliminate the
    // width/height arguments from the draw method.
    if ((aWidth != mWidth) || (aHeight != mHeight)) {
        aWidth = mWidth;
        aHeight = mHeight;
    }

#ifdef TRACE_IMAGE_ALLOCATION
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw:this=%p,x=%d,y=%d,width=%d,height=%d\n",
            this, aX, aY, aWidth, aHeight));
#endif

    nsDrawingSurfaceQT *drawing = (nsDrawingSurfaceQT*)aSurface;

    // Render unique image bits onto an off screen pixmap only once
    // The image bits can change as a result of ImageUpdated() - for
    // example: animated GIFs.
    if (nsnull == mImagePixmap) {
      CreateImagePixmap(aWidth,aHeight);
    }
    if (nsnull == mImagePixmap)
        return NS_ERROR_FAILURE;

    drawing->GetGC()->drawImage(aX, aY, *mImagePixmap, 0, 0, aWidth, aHeight);
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw:x=%d, y=%d, width=%d, height=%d, imagesize=%d",
            aX, aY, aWidth, aHeight, mSizeImage));

    return NS_OK;
}

void nsImageQT::CreateImagePixmap(PRInt32 aWidth,PRInt32 aHeight)
{
  mImagePixmap = new QImage(aWidth, aHeight, mDepth);
  mImagePixmap->setAlphaBuffer(PR_TRUE);

  if (mImagePixmap) {
    PRInt8 BytesPerPixel = mRequestDepth / 8;
    PRUint8 *alpha = mAlphaBits;

    for (PRInt32 i = 0; i < aHeight; i++) {
      QRgb * line = (QRgb*)mImagePixmap->scanLine(i);

      for (PRInt32 j = 0, l = 0; l < aWidth; j += BytesPerPixel, l++) {
        PRUint32 c = 0;
        PRUint8 tmp;

        for (PRInt8 k = (BytesPerPixel - 1); k >= 0; k--) {
          c <<= 8;
          tmp = mImageBits[(i * mRowBytes) + j + k];
          c |= tmp;
        }
        PRUint8 a;
        PRUint8 r = NS_GET_R(c);
        PRUint8 g = NS_GET_G(c);
        PRUint8 b = NS_GET_B(c);

        if (mAlphaBits)
          a = mAlphaDepth == 1 ? ((alpha[l / 8] & (1 << (7 - (l % 8)))) ? 255 : 0) : alpha[l];
        else if (BytesPerPixel == 4)
          a = NS_GET_A(c);
        else
          a = 255;
                 
        QRgb qcolor = qRgba(r, g, b, a);
        line[l] = qcolor;
      }
      alpha += mAlphaRowBytes;
    }
  }
}

NS_IMETHODIMP nsImageQT::DrawTile(nsIRenderingContext &aContext,
                                  nsDrawingSurface aSurface,
                                  nsRect &aSrcRect, nsRect &aTileRect)
{
  nsDrawingSurfaceQT *drawing = (nsDrawingSurfaceQT*)aSurface;

  if (drawing->GetDepth() == 8 || mAlphaDepth == 8) {
    PRInt32 aY0 = aTileRect.y, aX0 = aTileRect.x;
    PRInt32 aY1 = aTileRect.y + aTileRect.height;
    PRInt32 aX1 = aTileRect.x + aTileRect.width;
 
    for (PRInt32 y = aY0; y < aY1; y += aSrcRect.height)
      for (PRInt32 x = aX0; x < aX1; x += aSrcRect.width)
        Draw(aContext,aSurface,x,y, PR_MIN(aSrcRect.width, aX1 - x),
             PR_MIN(aSrcRect.height, aY1 - y));
 
    return NS_OK;
  }
  QPixmap qPmap;

  // Render unique image bits onto an off screen pixmap only once
  // The image bits can change as a result of ImageUpdated() - for
  // example: animated GIFs.
  if (nsnull == mImagePixmap) {
    CreateImagePixmap(mWidth,mHeight);
  }
  if (nsnull == mImagePixmap)
    return NS_ERROR_FAILURE;

  qPmap.convertFromImage(*mImagePixmap);
  drawing->GetGC()->drawTiledPixmap(aTileRect.x,aTileRect.y,
                                    aTileRect.width,aTileRect.height,
                                    qPmap,aSrcRect.x,aSrcRect.y);
  return NS_OK;
}

NS_IMETHODIMP nsImageQT::DrawTile(nsIRenderingContext &aContext,
                                  nsDrawingSurface aSurface,
                                  PRInt32 aSXOffset, PRInt32 aSYOffset,
                                  const nsRect &aTileRect)
{
  nsRect srcRect(aSXOffset,aSYOffset,mWidth,mHeight);
  nsRect dstRect(aTileRect);

  return DrawTile(aContext, aSurface, srcRect, dstRect);
}

//------------------------------------------------------------

nsresult nsImageQT::Optimize(nsIDeviceContext* aContext)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Optimize()\n"));
    return NS_OK;
}

PRInt32 nsImageQT::GetBytesPix()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetBytesPix()\n"));
    return mNumBytesPixel;
}
 
PRBool nsImageQT::GetIsRowOrderTopToBottom()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetIsRowOrderTopToBottom()\n"));
    return mIsTopToBottom;
}

//------------------------------------------------------------
// lock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImageQT::LockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels.  Implement this if you need it.
NS_IMETHODIMP
nsImageQT::UnlockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *	Set the decoded dimens of the image
 */
NS_IMETHODIMP
nsImageQT::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
    
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 
  return NS_OK;
}

