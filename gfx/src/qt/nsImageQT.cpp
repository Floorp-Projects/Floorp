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

#define IsFlagSet(a,b) (a & b)

#undef CHEAP_PERFORMANCE_MEASUREMENT

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

//------------------------------------------------------------

nsImageQT::nsImageQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::nsImageQT()\n"));
    NS_INIT_REFCNT();
    mImageBits   = nsnull;
    mWidth       = 0;
    mHeight      = 0;
    mDepth       = 0;
    mAlphaBits   = nsnull;
    mAlphaPixmap = nsnull;
    mImagePixmap = nsnull;
    mNaturalWidth = 0;
    mNaturalHeight = 0;

}

//------------------------------------------------------------

nsImageQT::~nsImageQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::~nsImageQT()\n"));
    if(nsnull != mImageBits) 
    {
        delete[] (PRUint8*)mImageBits;
        mImageBits = nsnull;
    }

    if (nsnull != mAlphaBits) 
    {
        delete[] (PRUint8*)mAlphaBits;
        mAlphaBits = nsnull;
        if (nsnull != mAlphaPixmap) 
        {
            delete mAlphaPixmap;
        }
    }

    if (nsnull != mImagePixmap)
    {
        delete mImagePixmap;
    }
}
NS_IMPL_ISUPPORTS(nsImageQT, kIImageIID);

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
    if ((aWidth == 0) && (aHeight == 0))
    {
        return NS_ERROR_FAILURE;
    }

    SetDecodedRect(0,0,0,0);  //init
    SetNaturalWidth(0);
    SetNaturalHeight(0);

    if (nsnull != mImageBits) 
    {
        delete[] (PRUint8*)mImageBits;
        mImageBits = nsnull;
    }

    if (nsnull != mAlphaBits) 
    {
        delete[] (PRUint8*)mAlphaBits;
        mAlphaBits = nsnull;
        if (nsnull != mAlphaPixmap) 
        {
            delete mAlphaPixmap;
            mAlphaPixmap = nsnull;
        }
    }

    // mImagePixmap gets created once per unique image bits in Draw()
    // ImageUpdated(nsImageUpdateFlags_kBitsChanged) can cause the
    // image bits to change and mImagePixmap will be unrefed and nulled.
    if (nsnull != mImagePixmap) 
    {
        delete mImagePixmap;
        mImagePixmap = nsnull;
    }

#if 1
    if (32 == aDepth)
    {
        mNumBytesPixel = 4;
    }
#else
    if (24 == aDepth) 
    {
        mNumBytesPixel = 3;
    }
#endif
    else 
    {
        NS_ASSERTION(PR_FALSE, "unexpected image depth");
        return NS_ERROR_UNEXPECTED;
    }

    mWidth = aWidth;
    mHeight = aHeight;
    mDepth = aDepth;
    mIsTopToBottom = PR_TRUE;

    // create the memory for the image
    ComputeMetrics();

    //mSizeImage = 8 * mWidth * mHeight;

    mImageBits = (PRUint8*) new PRUint8[mSizeImage];

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

        mAlphaBits = new unsigned char[mAlphaRowBytes * aHeight];
        mAlphaWidth = aWidth;
        mAlphaHeight = aHeight;
        break;

    case nsMaskRequirements_kNeeds8Bit:
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Init: 8 bit mask needed\n"));
        mAlphaBits = nsnull;
        mAlphaWidth = 0;
        mAlphaHeight = 0;
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsImageQT::Init: TODO want an 8bit mask for an image...\n"));
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
    if (nsnull == aSurface)
    {
        return NS_ERROR_FAILURE;
    }

    // Copy data from mImageBits to the drawing surface specified by aSurface.
    // We only want a subset of the data in mImageBits, and the data might
    // need to be scaled.

    nsDrawingSurfaceQT * drawing = (nsDrawingSurfaceQT *) aSurface;

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
    if (nsnull == aSurface)
    {
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::Draw: no surface!!\n"));
        return NS_ERROR_FAILURE;
    }

    // XXX kipp: this is temporary code until we eliminate the
    // width/height arguments from the draw method.
    if ((aWidth != mWidth) || (aHeight != mHeight)) 
    {
        aWidth = mWidth;
        aHeight = mHeight;
    }

#ifdef TRACE_IMAGE_ALLOCATION
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw:this=%p,x=%d,y=%d,width=%d,height=%d\n",
            this,
            aX,
            aY,
            aWidth,
            aHeight));
#endif

    nsDrawingSurfaceQT * drawing = (nsDrawingSurfaceQT *) aSurface;

    // Render unique image bits onto an off screen pixmap only once
    // The image bits can change as a result of ImageUpdated() - for
    // example: animated GIFs.
    if (nsnull == mImagePixmap)
    {
        PRInt32 depth = 32;//drawing->GetPixmap()->depth();
        mImagePixmap = new QImage(aWidth,
                                  aHeight,
                                  32);//drawing->GetPixmap()->depth());

        PRInt32 bytesperpixel = depth / 8;

        PRUint32 * mImagePixels = new PRUint32[mSizeImage / bytesperpixel];

        memcpy(mImagePixels, mImageBits, mSizeImage);

        if (mImagePixmap)
        {
            for (PRInt32 i = 0; i < aHeight; i++)
            {
                QRgb * line = (QRgb *) mImagePixmap->scanLine(i);

                for (PRInt32 j = 0; j < aWidth; j++)
                {
                    //line[j] = mImageBits[(i * aWidth) + j];
                    PRUint32 c = mImagePixels[(i * aWidth) + j];
                    PRUint8 r = NS_GET_R(c);
                    PRUint8 g = NS_GET_G(c);
                    PRUint8 b = NS_GET_B(c);
                    PRUint8 a = NS_GET_A(c);
                    //debug("nsColor=%4x, r=%x, g=%x, b=%x, a=%x", c, a, b, g, r);
                    //QRgb qcolor = qRgb(r, b, g);
                    QRgb qcolor = qRgba(a, b, g, r);
                    line[j] = qcolor;
                }
            }
        }
    }

    drawing->GetGC()->drawImage(aX, 
                                aY, 
                                *mImagePixmap, 
                                0, 
                                0, 
                                aWidth, 
                                aHeight);
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsImageQT::Draw:x=%d, y=%d, width=%d, height=%d, imagesize=%d",
            aX,
            aY,
            aWidth,
            aHeight,
            mSizeImage));

    return NS_OK;
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
    return 1;
}
 
PRBool nsImageQT::GetIsRowOrderTopToBottom()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsImageQT::GetIsRowOrderTopToBottom()\n"));
    return PR_TRUE;
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

