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

#include "nsImageBeOS.h"
#include "nsRenderingContextBeOS.h"

#include "nspr.h"
//#include <string.h>

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

      mAlphaBits = new PRUint8[8 * mAlphaRowBytes * aHeight]; 
	  memset(mAlphaBits, 255, 8*mAlphaRowBytes * aHeight);
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
	if((aWidth != mWidth) || (aHeight != mHeight))
	{
		aWidth = mWidth;
		aHeight = mHeight;
	}
	
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
		view->DrawBitmapAsync(mImage, BRect(aX, aY, aX + aWidth - 1, aY + aHeight - 1));
		view->SetDrawingMode(B_OP_COPY);
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

