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

#include "nsImageQt.h"
#include "nsRenderingContextQt.h"

#include "nspr.h"
#include "qtlog.h"

#define IsFlagSet(a,b) ((a) & (b))

#ifdef DEBUG
PRUint32 gImageCount = 0;
PRUint32 gImageID = 0;
#endif

NS_IMPL_ISUPPORTS1(nsImageQt, nsIImage)

//------------------------------------------------------------
nsImageQt::nsImageQt()
    : mWidth(0)
    , mHeight(0)
    , mDepth(0)
    , mRowBytes(0)
    , mImageBits(nsnull)
    , mAlphaDepth(0)
    , mAlphaRowBytes(0)
    , mAlphaBits(nsnull)
    , mNumBytesPixel(0)
    , pixmapDirty(PR_FALSE)
{
#ifdef DEBUG
    gImageCount++;
    mID = gImageID++;
    PR_LOG(gQtLogModule, QT_BASIC,
           ("nsImageQt CTOR (%p) ID: %d, Count: %d\n", this, mID, gImageCount));
#endif
}

//------------------------------------------------------------
nsImageQt::~nsImageQt()
{
#ifdef DEBUG
    gImageCount--;
    PR_LOG(gQtLogModule, QT_BASIC,
           ("nsImageQt DTOR (%p) ID: %d, Count: %d\n", this, mID, gImageCount));
#endif
    if (nsnull != mImageBits) {
        delete[] mImageBits;
        mImageBits = nsnull;
    }
    if (nsnull != mAlphaBits) {
        delete[] mAlphaBits;
        mAlphaBits = nsnull;
    }
}

//------------------------------------------------------------
nsresult nsImageQt::Init(PRInt32 aWidth,PRInt32 aHeight,
                         PRInt32 aDepth,
                         nsMaskRequirements aMaskRequirements)
{
    // gfxImageFrame forces only one nsImageQt::Init
    if (aWidth == 0 || aHeight == 0) {
        return NS_ERROR_FAILURE;
    }

//     if (32 == aDepth) {
//         mNumBytesPixel = 4;
//         mDepth = aDepth;
//     }
//     else
    if (24 == aDepth) {
        mNumBytesPixel = 3;
        mDepth = aDepth;
    }
    else {
        NS_NOTREACHED("unexpected image depth");
        return NS_ERROR_UNEXPECTED;
    }
    mWidth = aWidth;
    mHeight = aHeight;
    mRowBytes = (mWidth*mDepth/8 + 3) & ~0x3;

    switch (aMaskRequirements) {
    case nsMaskRequirements_kNeeds1Bit:
        mAlphaRowBytes = (aWidth + 7) / 8;
        mAlphaDepth = 1;
        // 32-bit align each row
        mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
        break;

    case nsMaskRequirements_kNeeds8Bit:
        mAlphaRowBytes = aWidth;
        mAlphaDepth = 8;
        // 32-bit align each row
        mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;
        break;
    default:
        mAlphaDepth = 0;
        mAlphaRowBytes = 0;
        break;
    }

    mImageBits = (PRUint8*)new PRUint8[mRowBytes * mHeight];
    mAlphaBits = new PRUint8[mAlphaRowBytes * mHeight];

    pixmapDirty = PR_TRUE;

    PR_LOG(gQtLogModule, QT_BASIC, ("nsImageQt::Init succeeded"));
    return NS_OK;
}

//------------------------------------------------------------

PRInt32 nsImageQt::GetHeight()
{
    return mHeight;
}

PRInt32 nsImageQt::GetWidth()
{
    return mWidth;
}

PRUint8 *nsImageQt::GetBits()
{
    return mImageBits;
}

void *nsImageQt::GetBitInfo()
{
    return nsnull;
}

PRInt32 nsImageQt::GetLineStride()
{
    return mRowBytes;
}

nsColorMap *nsImageQt::GetColorMap()
{
    return nsnull;
}

PRUint8 *nsImageQt::GetAlphaBits()
{
    return mAlphaBits;
}

PRInt32 nsImageQt::GetAlphaLineStride()
{
    return mAlphaRowBytes;
}

//------------------------------------------------------------

// set up the palette to the passed in color array, RGB only in this array
void nsImageQt::ImageUpdated(nsIDeviceContext *aContext,
                             PRUint8 aFlags,nsRect *aUpdateRect)
{
    //qDebug("image updated");
    if (IsFlagSet(nsImageUpdateFlags_kBitsChanged,aFlags))
        pixmapDirty = PR_TRUE;

    mDecodedRect.UnionRect(mDecodedRect, *aUpdateRect);
}

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP nsImageQt::Draw(nsIRenderingContext &aContext,
                              nsIDrawingSurface *aSurface,
                              PRInt32 aSX, PRInt32 aSY,
                              PRInt32 aSWidth, PRInt32 aSHeight,
                              PRInt32 aDX, PRInt32 aDY,
                              PRInt32 aDWidth, PRInt32 aDHeight)
{
    // qDebug("draw x = %d, y = %d, w = %d, h = %d, dx = %d, dy = %d, dw = %d, dy = %d",
    //         aSX, aSY, aSWidth, aSHeight, aDX, aDY, aDWidth, aDHeight);
    if (aSWidth <= 0 || aDWidth <= 0 || aSHeight <= 0 || aDHeight <= 0)
      return NS_OK;
    if (nsnull == aSurface) {
        return NS_ERROR_FAILURE;
    }
    if (pixmapDirty)
        updatePixmap();

    nsDrawingSurfaceQt *drawing = (nsDrawingSurfaceQt*)aSurface;

    // The code below seems to be wrong and not what gecko expects.
//     if ((aSWidth != aDWidth || aSHeight != aDHeight)
//         && (aSWidth != mWidth || aSHeight != mHeight)) {
//         QPixmap tmp(aSWidth, aSHeight);
//         copyBlt(&tmp, 0, 0, &pixmap, aSX, aSY, aSWidth, aSHeight);
//         drawing->GetGC()->drawPixmap(aDX, aDY, pixmap, aSX, aSY, aSWidth, aSHeight);
//     }

    nsRect sourceRect(aSX, aSY, aSWidth, aSHeight);
    if (!sourceRect.IntersectRect(sourceRect, mDecodedRect))
        return NS_OK;

    // Now get the part of the image that should be drawn
    // Copy into a new image so we can scale afterwards
    QImage image(pixmap.convertToImage().copy(sourceRect.x, sourceRect.y,
                                              sourceRect.width, sourceRect.height));

    if (image.isNull())
        return NS_ERROR_FAILURE;

    // Find the scale factor
    float w_factor = (float)aDWidth / aSWidth;
    float h_factor = (float)aDHeight / aSHeight;

    // If we had to draw only part of the requested image, must adjust
    // destination coordinates
    aDX += PRInt32((sourceRect.x - aSX) * w_factor);
    aDY += PRInt32((sourceRect.y - aSY) * h_factor);
    image = image.scale(int(sourceRect.width * w_factor), int(sourceRect.height * h_factor));
    drawing->GetGC()->drawImage(QPoint(aDX, aDY), image);

    //drawing->GetGC()->drawPixmap(aDX, aDY, pixmap, aSX, aSY, aSWidth, aSHeight);


    return NS_OK;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP nsImageQt::Draw(nsIRenderingContext &aContext,
                              nsIDrawingSurface *aSurface,
                              PRInt32 aX, PRInt32 aY,
                              PRInt32 aWidth, PRInt32 aHeight)
{
    return Draw(aContext, aSurface, 0, 0, mWidth, mHeight, aX, aY, aWidth, aHeight);
}

void nsImageQt::updatePixmap()
{
    //qDebug("updatePixmap");
    QImage qimage(mWidth, mHeight, 32);
    const PRInt32 bytesPerPixel = mDepth / 8;
    PRUint8 *image = mImageBits;
    PRUint8 *alpha = mAlphaBits;

    PRInt32 i,j;
    QRgb *line;

    qimage.setAlphaBuffer(mAlphaDepth != 0);
    switch(mAlphaDepth) {
    case 0:
        for (i = 0; i < mHeight; i++) {
            line = (QRgb*)qimage.scanLine(i);

            PRUint8 *imagePtr = image;
            for (j = 0; j < mWidth; j++) {
                line[j] = qRgb(*imagePtr, *(imagePtr+1), *(imagePtr+2));
                imagePtr += bytesPerPixel;
            }
            image += mRowBytes;
        }
        break;
    case 1:
        for (i = 0; i < mHeight; i++) {
            line = (QRgb*)qimage.scanLine(i);

            PRUint8 *imagePtr = image;
            for (j = 0; j < mWidth; j++) {
                uchar a = (alpha[j / 8] & (1 << (7 - (j % 8)))) ? 0xff : 0;
                line[j] = qRgba(*imagePtr, *(imagePtr+1), *(imagePtr+2), a);
                imagePtr += bytesPerPixel;
            }
            image += mRowBytes;
            alpha += mAlphaRowBytes;
        }
        break;
    case 8:
        for (i = 0; i < mHeight; i++) {
            line = (QRgb*)qimage.scanLine(i);

            PRUint8 *imagePtr = image;
            PRUint8 *alphaPtr = alpha;
            for (j = 0; j < mWidth; j++) {
                line[j] = qRgba(*imagePtr, *(imagePtr+1), *(imagePtr+2), *alphaPtr);
                imagePtr += bytesPerPixel;
                alphaPtr++;
            }
            image += mRowBytes;
            alpha += mAlphaRowBytes;
        }
        break;
    }

    pixmap = QPixmap(qimage);
    pixmapDirty = PR_FALSE;
}

NS_IMETHODIMP nsImageQt::DrawTile(nsIRenderingContext &aContext,
                                  nsIDrawingSurface *aSurface,
                                  PRInt32 aSXOffset, PRInt32 aSYOffset,
                                  PRInt32 aPadX, PRInt32 aPadY,
                                  const nsRect &aTileRect)
{
    //qDebug("draw tile");
#if 1
    nsRect aSrcRect(aSXOffset, aSYOffset, mWidth, mHeight);

    nsDrawingSurfaceQt *drawing = (nsDrawingSurfaceQt*)aSurface;

    if (aTileRect.width <= 0 || aTileRect.height <= 0) {
        NS_ASSERTION(aTileRect.width > 0 && aTileRect.height > 0,
                     "Error: image has 0 width or height!");
        return NS_OK;
    }
    if (drawing->GetDepth() == 8 || mAlphaDepth == 8) {
        PRInt32 aY0 = aTileRect.y, aX0 = aTileRect.x;
        PRInt32 aY1 = aTileRect.y + aTileRect.height;
        PRInt32 aX1 = aTileRect.x + aTileRect.width;

        for (PRInt32 y = aY0; y < aY1; y += aSrcRect.height + aPadY)
            for (PRInt32 x = aX0; x < aX1; x += aSrcRect.width + aPadX)
                Draw(aContext, aSurface, x, y, PR_MIN(aSrcRect.width, aX1 - x),
                     PR_MIN(aSrcRect.height, aY1 - y));

        return NS_OK;
    }
#if 0
    // Render unique image bits onto an off screen pixmap only once
    // The image bits can change as a result of ImageUpdated() - for
    // example: animated GIFs.
    if (nsnull == mImagePixmap) {
        CreateImagePixmap();
    }
    if (nsnull == mImagePixmap)
        return NS_ERROR_FAILURE;

    QPixmap qPmap;

    qPmap.convertFromImage(*mImagePixmap);
#endif
    /*qDebug("draw tilePixmap x = %d, y = %d, wid = %d, hei = %d, sx = %d, sy =  %d",
           aTileRect.x, aTileRect.y, aTileRect.width, aTileRect.height,
           aSrcRect.x, aSrcRect.y);*/
    drawing->GetGC()->drawTiledPixmap(aTileRect.x, aTileRect.y,
                                      aTileRect.width, aTileRect.height,
                                      pixmap, aSrcRect.x, aSrcRect.y);
    //qPmap, aSrcRect.x, aSrcRect.y);
#endif
    return NS_OK;
}

//------------------------------------------------------------

nsresult nsImageQt::Optimize(nsIDeviceContext* aContext)
{
    return NS_OK;
}

PRInt32 nsImageQt::GetBytesPix()
{
    return mNumBytesPixel;
}

//------------------------------------------------------------
// lock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImageQt::LockImagePixels(PRBool aMaskPixels)
{
    return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels.  Implement this if you need it.
NS_IMETHODIMP
nsImageQt::UnlockImagePixels(PRBool aMaskPixels)
{
    return NS_OK;
}

NS_IMETHODIMP nsImageQt::DrawToImage(nsIImage* aDstImage,
                                      nscoord aDX, nscoord aDY,
                                      nscoord aDWidth, nscoord aDHeight)
{
    //qDebug("DrawToIMAGE");
  nsImageQt *dest = NS_STATIC_CAST(nsImageQt *, aDstImage);

  if (!dest)
    return NS_ERROR_FAILURE;

  if (aDX >= dest->mWidth || aDY >= dest->mHeight)
    return NS_OK;

  PRUint8 *rgbPtr=0, *alphaPtr=0;
  PRUint32 rgbStride, alphaStride;

  rgbPtr = mImageBits;
  rgbStride = mRowBytes;
  alphaPtr = mAlphaBits;
  alphaStride = mAlphaRowBytes;

  PRInt32 y;
  PRInt32 ValidWidth = ( aDWidth < ( dest->mWidth - aDX ) ) ? aDWidth : ( dest->mWidth - aDX );
  PRInt32 ValidHeight = ( aDHeight < ( dest->mHeight - aDY ) ) ? aDHeight : ( dest->mHeight - aDY );

  // now composite the two images together
  switch (mAlphaDepth) {
  case 1:
    {
      PRUint8 *dst = dest->mImageBits + aDY*dest->mRowBytes + 3*aDX;
      PRUint8 *dstAlpha = dest->mAlphaBits + aDY*dest->mAlphaRowBytes;
      PRUint8 *src = rgbPtr;
      PRUint8 *alpha = alphaPtr;
      PRUint8 offset = aDX & 0x7; // x starts at 0
      int iterations = (ValidWidth+7)/8; // round up

      for (y=0; y<ValidHeight; y++) {
        for (int x=0; x<ValidWidth; x += 8, dst += 3*8, src += 3*8) {
          PRUint8 alphaPixels = *alpha++;
          if (alphaPixels == 0) {
            // all 8 transparent; jump forward
            continue;
          }

          // 1 or more bits are set, handle dstAlpha now - may not be aligned.
          // Are all 8 of these alpha pixels used?
          if (x+7 >= ValidWidth) {
            alphaPixels &= 0xff << (8 - (ValidWidth-x)); // no, mask off unused
            if (alphaPixels == 0)
              continue;  // no 1 alpha pixels left
          }
          if (offset == 0) {
            dstAlpha[(aDX+x)>>3] |= alphaPixels; // the cheap aligned case
          }
          else {
            dstAlpha[(aDX+x)>>3]       |= alphaPixels >> offset;
            // avoid write if no 1's to write - also avoids going past end of array
            PRUint8 alphaTemp = alphaPixels << (8U - offset);
            if (alphaTemp & 0xff)
              dstAlpha[((aDX+x)>>3) + 1] |= alphaTemp;
          }

          if (alphaPixels == 0xff) {
            // fix - could speed up by gathering a run of 0xff's and doing 1 memcpy
            // all 8 pixels set; copy and jump forward
            memcpy(dst,src,8*3);
            continue;
          }
          else {
            // else mix of 1's and 0's in alphaPixels, do 1 bit at a time
            // Don't go past end of line!
            PRUint8 *d = dst, *s = src;
            for (PRUint8 aMask = 1<<7, j = 0; aMask && j < ValidWidth-x; aMask >>= 1, j++) {
              // if this pixel is opaque then copy into the destination image
              if (alphaPixels & aMask) {
                // might be faster with *d++ = *s++ 3 times?
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                // dstAlpha bit already set
              }
              d += 3;
              s += 3;
            }
          }
        }
        // at end of each line, bump pointers.  Use wordy code because of
        // bug 127455 to avoid possibility of unsigned underflow
        dst = (dst - 3*8*iterations) + dest->mRowBytes;
        src = (src - 3*8*iterations) + rgbStride;
        alpha = (alpha - iterations) + alphaStride;
        dstAlpha += dest->mAlphaRowBytes;
      }
    }
    break;
  case 0:
  default:
    for (y=0; y<ValidHeight; y++)
      memcpy(dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX,
             rgbPtr + y*rgbStride,
             3*ValidWidth);
  }

  nsRect rect(aDX, aDY, ValidWidth, ValidHeight);
  dest->ImageUpdated(nsnull, 0, &rect);

  return NS_OK;
}
