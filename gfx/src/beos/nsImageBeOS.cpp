/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsImageBeOS.h"
#include "nsRenderingContextBeOS.h"

#include "nspr.h"
//#include <string.h>

#include "imgScaler.h"

#define IsFlagSet(a,b) (a & b)

NS_IMPL_ISUPPORTS1(nsImageBeOS, nsIImage)

//------------------------------------------------------------

nsImageBeOS::nsImageBeOS()
{
  NS_INIT_REFCNT();
  mStaticImage = PR_FALSE;
  mImageBits = nsnull;
  mWidth = 0;
  mHeight = 0;
  mDepth = 0;
  mAlphaBits = nsnull;
  mAlphaDepth = 0; 
  mRowBytes = 0; 
  mSizeImage = 0; 
  mAlphaHeight = 0; 
  mAlphaWidth = 0;
  mImage = nsnull;
  mNaturalWidth = 0;
  mNaturalHeight = 0;

}

//------------------------------------------------------------

nsImageBeOS::~nsImageBeOS()
{
  if (NULL != mImage) {
    delete mImage;
    mImage = NULL;
  }

  if(nsnull != mImageBits) {
    delete[] (PRUint8*)mImageBits;
    mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
  }
}

//------------------------------------------------------------

nsresult
 nsImageBeOS::Init(PRInt32 aWidth, PRInt32 aHeight,
                  PRInt32 aDepth, nsMaskRequirements aMaskRequirements)
{
  if (nsnull != mImageBits) {
   delete[] (PRUint8*)mImageBits;
   mImageBits = nsnull;
  }

  if (nsnull != mAlphaBits) {
    delete[] (PRUint8*)mAlphaBits;
    mAlphaBits = nsnull;
  }

  SetDecodedRect(0,0,0,0);  //init 
  SetNaturalWidth(0); 
  SetNaturalHeight(0); 

  if (24 == aDepth) {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;
  mIsTopToBottom = PR_TRUE; 

  // create the memory for the image
  ComputeMetrics();

  mImageBits = (PRUint8*) new PRUint8[mSizeImage];

  switch(aMaskRequirements)
  {
    case nsMaskRequirements_kNoMask:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
      break;

    case nsMaskRequirements_kNeeds1Bit:
      mAlphaRowBytes = (aWidth + 7) / 8;
      mAlphaDepth = 1;

      // 32-bit align each row
      mAlphaRowBytes = (mAlphaRowBytes + 3) & ~0x3;

      mAlphaBits = new PRUint8[mAlphaRowBytes * aHeight]; 
	  memset(mAlphaBits, 255, mAlphaRowBytes * aHeight);
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;

    case nsMaskRequirements_kNeeds8Bit:
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

PRInt32 nsImageBeOS::GetHeight()
{
  return mHeight;
}

PRInt32 nsImageBeOS::GetWidth()
{
  return mWidth;
}

PRUint8 *nsImageBeOS::GetBits()
{
  return mImageBits;
}

void *nsImageBeOS::GetBitInfo()
{
  return nsnull;
}

PRInt32 nsImageBeOS::GetLineStride()
{
  return mRowBytes;
}

nsColorMap *nsImageBeOS::GetColorMap()
{
  return nsnull;
}

PRBool nsImageBeOS::IsOptimized()
{
  return mStaticImage;
}

PRUint8 *nsImageBeOS::GetAlphaBits()
{
  return mAlphaBits;
}

PRInt32 nsImageBeOS::GetAlphaWidth()
{
  return mAlphaWidth;
}

PRInt32 nsImageBeOS::GetAlphaHeight()
{
  return mAlphaHeight;
}

PRInt32 nsImageBeOS::GetAlphaLineStride()
{
  return mAlphaRowBytes;
}

nsIImage *nsImageBeOS::DuplicateImage()
{
	return nsnull;
}

void nsImageBeOS::SetAlphaLevel(PRInt32 aAlphaLevel)
{
}

PRInt32 nsImageBeOS::GetAlphaLevel()
{
	return 0;
}

void nsImageBeOS::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
}

//------------------------------------------------------------

// set up the palette to the passed in color array, RGB only in this array
void
nsImageBeOS::ImageUpdated(nsIDeviceContext *aContext,
                         PRUint8 aFlags,
                         nsRect *aUpdateRect)
{
  mFlags = aFlags; // this should be 0'd out by Draw() 
 
} 

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageBeOS::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
	nsDrawingSurfaceBeOS	*beosdrawing =(nsDrawingSurfaceBeOS *) aSurface;
	BView	*view;

       beosdrawing->AcquireView(&view); 
	
	if((PR_FALSE == mStaticImage) || (NULL == mImage))
		BuildImage(aSurface);
	
	if(NULL == mImage)
		return PR_FALSE;
	
	if(view && view->LockLooper())
	{
		view->SetDrawingMode(B_OP_ALPHA);
		view->DrawBitmapAsync(mImage, BRect(aSX, aSY, aSX + aSWidth - 1, aSY + aSHeight - 1),
			BRect(aDX, aDY, aDX + aDWidth - 1, aDY + aDHeight - 1));
		view->SetDrawingMode(B_OP_COPY);
		view->Sync();
		view->UnlockLooper();
	}
	beosdrawing->ReleaseView();

	return NS_OK;
}

//------------------------------------------------------------

// Draw the bitmap, this draw just has destination coordinates
NS_IMETHODIMP
nsImageBeOS::Draw(nsIRenderingContext &aContext,
                 nsDrawingSurface aSurface,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight)
{
  // XXX kipp: this is temporary code until we eliminate the
  // width/height arguments from the draw method.
  aWidth = PR_MIN(aWidth, mWidth);
  aHeight = PR_MIN(aHeight, mHeight);
	
  nsDrawingSurfaceBeOS	*beosdrawing = (nsDrawingSurfaceBeOS *) aSurface;
  BView	*view;
       
  beosdrawing->AcquireView(&view); 

  if((PR_FALSE == mStaticImage) || (NULL == mImage))
    BuildImage(aSurface);

  if(NULL == mImage)
    return PR_FALSE;

  if(view && view->LockLooper())
  {
    view->SetDrawingMode(B_OP_ALPHA);
    view->DrawBitmapAsync(mImage,
                          BRect(0, 0, aWidth - 1, aHeight - 1),
                          BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
    view->SetDrawingMode(B_OP_COPY);
    view->Sync();
    view->UnlockLooper();
  }
  beosdrawing->ReleaseView();

  return NS_OK;
}

/** 
 * Draw a tiled version of the bitmap 
 * @update - dwc 3/30/00 
 * @param aSurface  the surface to blit to 
 * @param aX0 starting x 
 * @param aY0 starting y 
 * @param aX1 ending x 
 * @param aY1 ending y 
 * @param aWidth The destination width of the pixelmap 
 * @param aHeight The destination height of the pixelmap 
 */ 
NS_IMETHODIMP nsImageBeOS::DrawTile(nsIRenderingContext &aContext, 
                                   nsDrawingSurface aSurface, 
                                   nsRect &aSrcRect, 
                                   nsRect &aTileRect) 
{ 
  PRInt32 aY0 = aTileRect.y, 
          aX0 = aTileRect.x, 
          aY1 = aTileRect.y + aTileRect.height, 
          aX1 = aTileRect.x + aTileRect.width; 
 
          for (PRInt32 y = aY0; y < aY1; y+=aSrcRect.height) 
            for (PRInt32 x = aX0; x < aX1; x+=aSrcRect.width) 
              Draw(aContext,aSurface,x,y, 
                   PR_MIN(aSrcRect.width, aX1-x), 
                   PR_MIN(aSrcRect.height, aY1-y)); 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsImageBeOS::DrawTile(nsIRenderingContext &aContext, 
                                   nsDrawingSurface aSurface, 
                                   PRInt32 aSXOffset, PRInt32 aSYOffset, 
                                   const nsRect &aTileRect) 
{ 
  PRInt32 
    validX = 0, 
    validY = 0, 
    validWidth  = mWidth, 
    validHeight = mHeight; 
  
  // limit the image rectangle to the size of the image data which 
  // has been validated. 
  if ((mDecodedY2 < mHeight)) { 
    validHeight = mDecodedY2 - mDecodedY1; 
  } 
  if ((mDecodedX2 < mWidth)) { 
    validWidth = mDecodedX2 - mDecodedX1; 
  } 
  if ((mDecodedY1 > 0)) {   
    validHeight -= mDecodedY1; 
    validY = mDecodedY1; 
  } 
  if ((mDecodedX1 > 0)) { 
    validWidth -= mDecodedX1; 
    validX = mDecodedX1; 
  } 
 
  PRInt32 aY0 = aTileRect.y - aSYOffset, 
          aX0 = aTileRect.x - aSXOffset, 
          aY1 = aTileRect.y + aTileRect.height, 
          aX1 = aTileRect.x + aTileRect.width; 
 
  // Set up clipping and call Draw(). 
  PRBool clipState; 
  aContext.PushState(); 
  aContext.SetClipRect(aTileRect, nsClipCombine_kIntersect, 
                       clipState); 
 
  for (PRInt32 y = aY0; y < aY1; y+=validHeight) 
    for (PRInt32 x = aX0; x < aX1; x+=validWidth) 
      Draw(aContext,aSurface,x,y, PR_MIN(validWidth, aX1-x), 
           PR_MIN(validHeight, aY1-y)); 
 
  aContext.PopState(clipState); 
 
  return NS_OK; 
} 
 
nsresult nsImageBeOS::BuildImage(nsDrawingSurface aDrawingSurface)
{
	if(NULL != mImage)
	{
		delete mImage;
		mImage = NULL;
	}
	
//	ConvertImage(aDrawingSurface);
	CreateImage(aDrawingSurface);
	
	return NS_OK;
}

void nsImageBeOS::CreateImage(nsDrawingSurface aSurface)
{
	if(mImageBits)
	{
		color_space cs;
		if(mDepth == 8)
			cs = B_CMAP8;
		else
			cs = B_RGBA32;
		mImage = new BBitmap(BRect(0, 0, mWidth - 1, mHeight - 1), cs);

    PRInt32 span = (mWidth * mDepth) >> 5; 
 
    if (((PRUint32)mWidth * mDepth) & 0x1F) 
      span++; 
    span <<= 2; 
		if( mImage && mImage->IsValid() )
		{
			uint8 *dest = (uint8*)mImage->Bits();
			uint8 *src = mImageBits;
			uint8 *alpha = mAlphaBits;

			if(mAlphaBits)
			{
				if( mDepth == 32 )
				{
					uint32 *pdest, *psrc;
					for( int j=0; j<mHeight; j++ )
					{
						for( int i=0; i<mWidth; i++ )
						{
							pdest = (uint32*)dest; psrc = (uint32*)src;
							// ANSI Shit, can't cast the lvalue!
							*pdest = *psrc;
							dest[3] = mAlphaDepth == 1 ? ((alpha[i / 8] & (1 << (7 - (i % 8)))) ? 255 : 0) : alpha[i];
              dest += 4; 
              src+=4; 
						}
						src += span - (mWidth*mNumBytesPixel);
						alpha += mAlphaRowBytes;
					}
				}
				else if( mDepth == 8 )
				{
					for( int j=0; j<mHeight; j++ )
					{
						for( int i=0; i<mWidth * mNumBytesPixel; i++ )
							*dest++ = *src++;
						src += span - (mWidth*mNumBytesPixel);
					}
				}
				else if( mDepth == 24 )
				{
					for( int j=0; j<mHeight; j++ )
					{
						for( int i=0; i<mWidth; i++ )
						{
							dest[0] = src[0];
							dest[1] = src[1];
							dest[2] = src[2];
							dest[3] = mAlphaDepth == 1 ? ((alpha[i / 8] & (1 << (7 - (i % 8)))) ? 255 : 0) : alpha[i];
							dest+= 4;
							src+=3;
						}
						src += span - (mWidth*mNumBytesPixel);
						alpha += mAlphaRowBytes;
					}        
				}
			}
			else
			{
				if( mDepth == 32 )
				{
					uint32 *pdest, *psrc;
					for( int j=0; j<mHeight; j++ )
					{
						for( int i=0; i<mWidth; i++ )
						{
							pdest = (uint32*)dest; psrc = (uint32*)src;
							// ANSI Shit, can't cast the lvalue!
							*pdest = *psrc;
							dest += 4; src+=4;
						}
						src += span - (mWidth*mNumBytesPixel);
					}
				}
				else if( mDepth == 8 )
				{
					for( int j=0; j<mHeight; j++ )
					{
						for( int i=0; i<mWidth * mNumBytesPixel; i++ )
							*dest++ = *src++;
						src += span - (mWidth*mNumBytesPixel);
					}
				}
				else if( mDepth == 24 )
				{
					for( int j=0; j<mHeight; j++ )
					{
						for( int i=0; i<mWidth; i++ )
						{
							memcpy( dest, src, mNumBytesPixel );
							dest[3] = 255;
							dest+= 4;
							src+=3;
						}
						src += span - (mWidth*mNumBytesPixel);
					}        
				}
			}
		}
	}	
}

//------------------------------------------------------------

nsresult nsImageBeOS::Optimize(nsIDeviceContext* aContext)
{
  mStaticImage = PR_TRUE;
  return NS_OK;
}

//------------------------------------------------------------
// lock the image pixels. Implement this if you need it.
NS_IMETHODIMP
nsImageBeOS::LockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

//------------------------------------------------------------
// unlock the image pixels.  Implement this if you need it.
NS_IMETHODIMP
nsImageBeOS::UnlockImagePixels(PRBool aMaskPixels)
{
  return NS_OK;
}

// ---------------------------------------------------
//	Set the decoded dimens of the image
//
NS_IMETHODIMP
nsImageBeOS::SetDecodedRect(PRInt32 x1, PRInt32 y1, PRInt32 x2, PRInt32 y2 )
{
  mDecodedX1 = x1; 
  mDecodedY1 = y1; 
  mDecodedX2 = x2; 
  mDecodedY2 = y2; 
  return NS_OK;
}

#ifdef USE_IMG2
NS_IMETHODIMP nsImageBeOS::DrawToImage(nsIImage* aDstImage,
                                       nscoord aDX, nscoord aDY,
                                       nscoord aDWidth, nscoord aDHeight)
{
  nsImageBeOS *dest = NS_STATIC_CAST(nsImageBeOS *, aDstImage);

  if (!dest)
    return NS_ERROR_FAILURE;
  
  if (!dest->mImage)
    dest->CreateImage(NULL);
  
  if (!dest->mImage)
    return NS_ERROR_FAILURE;

  if (!mImage)
    CreateImage(NULL);
  
  if (!mImage)
    return NS_ERROR_FAILURE;
    
  BBitmap *bmpDst = dest->mImage;
  BBitmap bmpTmp(bmpDst->Bounds(), bmpDst->ColorSpace(), true);
  BView *v = new BView(bmpDst->Bounds(), "", 0, 0);
  if (!v)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy( bmpTmp.Bits(), bmpDst->Bits(), bmpDst->BitsLength() );
  bmpTmp.AddChild(v);
  
  bmpTmp.Lock();
  v->SetDrawingMode(B_OP_ALPHA);
  v->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
  v->DrawBitmap(mImage, BRect(0, 0, mWidth - 1, mHeight - 1), BRect( aDX, aDY, aDX + aDWidth - 1, aDY + aDHeight -1 ));
  v->SetDrawingMode(B_OP_COPY);
  v->Sync();
  bmpTmp.Unlock();
  bmpTmp.RemoveChild(v);
  bmpDst->SetBits(bmpTmp.Bits(), bmpTmp.BitsLength(), 0, bmpTmp.ColorSpace() );
  delete v;
  
  //
  // following part is derived from GTK version
  // 2001/6/21 Makoto Hamanaka < VTA04230@nifty.com >
  
  // need to copy the mImageBits in case we're rendered scaled
  PRUint8 *scaledImage = 0, *scaledAlpha = 0;
  PRUint8 *rgbPtr=0, *alphaPtr=0;
  PRUint32 rgbStride, alphaStride;

  if ((aDWidth != mWidth) || (aDHeight != mHeight)) {
    // scale factor in DrawTo... start scaling
    scaledImage = (PRUint8 *)nsMemory::Alloc(3*aDWidth*aDHeight);
    if (!scaledImage)
      return NS_ERROR_OUT_OF_MEMORY;

    RectStretch(0, 0, mWidth-1, mHeight-1, 0, 0, aDWidth-1, aDHeight-1,
                mImageBits, mRowBytes, scaledImage, 3*aDWidth, 24);

    if (mAlphaDepth) {
      if (mAlphaDepth==1)
        alphaStride = (aDWidth+7)>>3;    // round to next byte
      else
        alphaStride = aDWidth;

      scaledAlpha = (PRUint8 *)nsMemory::Alloc(alphaStride*aDHeight);
      if (!scaledAlpha) {
        nsMemory::Free(scaledImage);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      RectStretch(0, 0, mWidth-1, mHeight-1, 0, 0, aDWidth-1, aDHeight-1,
                  mAlphaBits, mAlphaRowBytes, scaledAlpha, alphaStride,
                  mAlphaDepth);
    }
    rgbPtr = scaledImage;
    rgbStride = 3*aDWidth;
    alphaPtr = scaledAlpha;
  } else {
    rgbPtr = mImageBits;
    rgbStride = mRowBytes;
    alphaPtr = mAlphaBits;
    alphaStride = mAlphaRowBytes;
  }

  PRInt32 y;
  // now composite the two images together
  switch (mAlphaDepth) {
  case 1:
    for (y=0; y<aDHeight; y++) {
      PRUint8 *dst = dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX;
      PRUint8 *dstAlpha = dest->mAlphaBits + (y+aDY)*dest->mAlphaRowBytes;
      PRUint8 *src = rgbPtr + y*rgbStride; 
      PRUint8 *alpha = alphaPtr + y*alphaStride;
      for (int x=0; x<aDWidth; x++, dst+=3, src+=3) {
#define NS_GET_BIT(rowptr, x) (rowptr[(x)>>3] &  (1<<(7-(x)&0x7)))
#define NS_SET_BIT(rowptr, x) (rowptr[(x)>>3] |= (1<<(7-(x)&0x7)))

        // if this pixel is opaque then copy into the destination image
        if (NS_GET_BIT(alpha, x)) {
          dst[0] = src[0];
          dst[1] = src[1];
          dst[2] = src[2];
          NS_SET_BIT(dstAlpha, aDX+x);
        }

#undef NS_GET_BIT
#undef NS_SET_BIT
      }
    }
    break;
  case 8:
    for (y=0; y<aDHeight; y++) {
      PRUint8 *dst = dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX;
      PRUint8 *dstAlpha = 
        dest->mAlphaBits + (y+aDY)*dest->mAlphaRowBytes + aDX;
      PRUint8 *src = rgbPtr + y*rgbStride; 
      PRUint8 *alpha = alphaPtr + y*alphaStride;
      for (int x=0; x<aDWidth; x++, dst+=3, dstAlpha++, src+=3, alpha++) {

        // blend this pixel over the destination image
        unsigned val = *alpha;
        MOZ_BLEND(dst[0], dst[0], src[0], val);
        MOZ_BLEND(dst[1], dst[1], src[1], val);
        MOZ_BLEND(dst[2], dst[2], src[2], val);
        MOZ_BLEND(*dstAlpha, *dstAlpha, val, val);
      }
    }
    break;
  case 0:
  default:
    for (y=0; y<aDHeight; y++)
      memcpy(dest->mImageBits + (y+aDY)*dest->mRowBytes + 3*aDX, 
             rgbPtr + y*rgbStride,
             3*aDWidth);
  }
  if (scaledAlpha)
    nsMemory::Free(scaledAlpha);
  if (scaledImage)
    nsMemory::Free(scaledImage);
  
  return NS_OK;

}
#endif
