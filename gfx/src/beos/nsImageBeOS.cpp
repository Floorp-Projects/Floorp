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
#include <string.h>

#define IsFlagSet(a,b) (a & b)

static NS_DEFINE_IID(kIImageIID, NS_IIMAGE_IID);

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
  mAlphaPixmap = nsnull;
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
    delete mAlphaPixmap;
  }
}

NS_IMPL_ISUPPORTS(nsImageBeOS, kIImageIID);

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
    delete mAlphaPixmap;
    mAlphaPixmap = nsnull;
  }

  if (24 == aDepth) {
    mNumBytesPixel = 3;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected image depth");
    return NS_ERROR_UNEXPECTED;
  }

  SetDecodedRect(0,0,0,0);  //init
  SetNaturalWidth(0);
  SetNaturalHeight(0);

  mWidth = aWidth;
  mHeight = aHeight;
  mDepth = aDepth;

  // create the memory for the image
  ComputMetrics();

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

      mAlphaBits = new unsigned char[8 * mAlphaRowBytes * aHeight];
	  memset(mAlphaBits, 255, 8*mAlphaRowBytes * aHeight);
      mAlphaWidth = aWidth;
      mAlphaHeight = aHeight;
      break;

    case nsMaskRequirements_kNeeds8Bit:
      mAlphaBits = nsnull;
      mAlphaWidth = 0;
      mAlphaHeight = 0;
printf("nsImageBeOS::Init: 8 bit image mask unimplemented\n");
      break;
  }

  return NS_OK;
}

//------------------------------------------------------------

void nsImageBeOS::ComputMetrics()
{
  mRowBytes = CalcBytesSpan(mWidth);
  mSizeImage = mRowBytes * mHeight;
}

PRInt32
nsImageBeOS::GetBytesPix()
{
  NS_NOTYETIMPLEMENTED("somebody called this thang");
  return 0;/* XXX */
}

PRInt32
nsImageBeOS::GetHeight()
{
  return mHeight;
}

PRInt32
nsImageBeOS::GetWidth()
{
  return mWidth;
}

PRUint8*
nsImageBeOS::GetBits()
{
  return mImageBits;
}

void*
nsImageBeOS::GetBitInfo()
{
  return nsnull;
}

PRInt32
nsImageBeOS::GetLineStride()
{
  return mRowBytes;
}

nsColorMap*
nsImageBeOS::GetColorMap()
{
  return nsnull;
}

PRBool
nsImageBeOS::IsOptimized()
{
  return mStaticImage;
}

PRUint8*
nsImageBeOS::GetAlphaBits()
{
  return mAlphaBits;
}

PRInt32
nsImageBeOS::GetAlphaWidth()
{
  return mAlphaWidth;
}

PRInt32
nsImageBeOS::GetAlphaHeight()
{
  return mAlphaHeight;
}

PRInt32
nsImageBeOS::GetAlphaLineStride()
{
  return mAlphaRowBytes;
}

nsIImage*
nsImageBeOS::DuplicateImage()
{
	printf("nsImageBeOS::DuplicateImage - FIXME: not implamented\n");
	return nsnull;
}

void
nsImageBeOS::SetAlphaLevel(PRInt32 aAlphaLevel)
{
	printf("nsImageBeOS::SetAlphaLevel - FIXME: not implamented\n");
}

PRInt32
nsImageBeOS::GetAlphaLevel()
{
	printf("nsImageBeOS::GetAlphaLevel - FIXME: not implamented\n");
	return 0;
}

void
nsImageBeOS::MoveAlphaMask(PRInt32 aX, PRInt32 aY)
{
	printf("nsImageBeOS::MoveAlphaMask - FIXME: not implamented\n");
}

//------------------------------------------------------------

PRInt32  nsImageBeOS::CalcBytesSpan(PRUint32  aWidth)
{
  PRInt32 spanbytes;

  spanbytes = (aWidth * mDepth) >> 5;

  if (((PRUint32)aWidth * mDepth) & 0x1F)
    spanbytes++;
  spanbytes <<= 2;
  return(spanbytes);
}

//------------------------------------------------------------

// set up the palette to the passed in color array, RGB only in this array
void
nsImageBeOS::ImageUpdated(nsIDeviceContext *aContext,
                         PRUint8 aFlags,
                         nsRect *aUpdateRect)
{
}

//------------------------------------------------------------

// Draw the bitmap, this method has a source and destination coordinates
NS_IMETHODIMP
nsImageBeOS::Draw(nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                 PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                 PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight)
{
	nsDrawingSurfaceBeOS	*beosdrawing =(nsDrawingSurfaceBeOS *) aSurface;
	BView	*view;
	beosdrawing->GetView(&view);
	
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
	beosdrawing->GetView(&view);

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
	PRUint32 wdepth;
	nsDrawingSurfaceBeOS	*beosdrawing = (nsDrawingSurfaceBeOS *) aSurface;
	
	if(mImageBits)
	{
		color_space cs;
		if(mDepth == 8)
			cs = B_CMAP8;
		else
			cs = B_RGBA32;
		mImage = new BBitmap(BRect(0, 0, mWidth - 1, mHeight - 1), cs);

		PRInt32 span = CalcBytesSpan(mWidth);
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
							dest += 4; src+=4;
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

