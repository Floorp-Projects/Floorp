/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxImageFrame.h"
#include "nsIServiceManager.h"
#include <limits.h>
#include "prmem.h"

NS_IMPL_ISUPPORTS2(gfxImageFrame, gfxIImageFrame, nsIInterfaceRequestor)

gfxImageFrame::gfxImageFrame() :
  mImageData(nsnull),
  mTimeout(100),
  mDisposalMethod(0), /* imgIContainer::kDisposeNotSpecified */
  mBlendMethod(1), /* imgIContainer::kBlendOver */
  mInitialized(PR_FALSE),
  mMutable(PR_TRUE)
{
  /* member initializers and constructor code */
}

gfxImageFrame::~gfxImageFrame()
{
  /* destructor code */
  PR_FREEIF(mImageData);
  mInitialized = PR_FALSE;
}

/* void init (in PRInt32 aX, in PRInt32 aY, in PRInt32 aWidth, in PRInt32 aHeight, in gfx_format aFormat); */
NS_IMETHODIMP gfxImageFrame::Init(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, gfx_format aFormat,gfx_depth aDepth)
{
  if (mInitialized)
    return NS_ERROR_FAILURE;

  // assert for properties that should be verified by decoders, warn for properties related to bad content
  
  if (aWidth <= 0 || aHeight <= 0) {
    NS_ASSERTION(0, "error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  /* check to make sure we don't overflow a 32-bit */
  PRInt32 tmp = aWidth * aHeight;
  if (tmp / aHeight != aWidth) {
    NS_WARNING("width or height too large");
    return NS_ERROR_FAILURE;
  }
  tmp = tmp * 4;
  if (tmp / 4 != aWidth * aHeight) {
    NS_WARNING("width or height too large");
    return NS_ERROR_FAILURE;
  }

  /* reject over-wide or over-tall images */
  const PRInt32 k64KLimit = 0x0000FFFF;
  if ( aWidth > k64KLimit || aHeight > k64KLimit ){
    NS_WARNING("image too big");
    return NS_ERROR_FAILURE;
  }
  
#if defined(XP_MACOSX)
  // CoreGraphics is limited to images < 32K in *height*, so clamp all surfaces on the Mac to that height
  if (aHeight > SHRT_MAX) {
    NS_WARNING("image too big");
    return NS_ERROR_FAILURE;
  }
#endif

  mOffset.MoveTo(aX, aY);
  mSize.SizeTo(aWidth, aHeight);

  mFormat = aFormat;
  mDepth = aDepth;

  PRBool needImage = PR_TRUE;
  nsMaskRequirements maskReq;

  switch (aFormat) {
  case gfxIFormats::BGR:
  case gfxIFormats::RGB:
    maskReq = nsMaskRequirements_kNoMask;
    break;

  case gfxIFormats::BGRA:
  case gfxIFormats::RGBA:
    maskReq = nsMaskRequirements_kNeeds8Bit;
    break;

  case gfxIFormats::BGR_A1:
  case gfxIFormats::RGB_A1:
    maskReq = nsMaskRequirements_kNeeds1Bit;
    break;

  case gfxIFormats::BGR_A8:
  case gfxIFormats::RGB_A8:
    maskReq = nsMaskRequirements_kNeeds8Bit;
    break;

  case gfxIFormats::PAL:
  case gfxIFormats::PAL_A1:
    needImage = PR_FALSE;
    break;

  default:
    NS_ERROR("unsupported gfx_format\n");
    return NS_ERROR_FAILURE;
  }

  if (needImage) {
    if (aDepth != 24) {
      NS_ERROR("This Depth is not supported");
      return NS_ERROR_FAILURE;
    }

    nsresult rv;
    mImage = do_CreateInstance("@mozilla.org/gfx/image;1", &rv);
    NS_ASSERTION(mImage, "creation of image failed");
    if (NS_FAILED(rv)) return rv;
  
    rv = mImage->Init(aWidth, aHeight, aDepth, maskReq);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    if ((aDepth < 1) || (aDepth > 8)) {
      NS_ERROR("This Depth is not supported\n");
      return NS_ERROR_FAILURE;
    }
    mImageData = (PRUint8*)PR_MALLOC(PaletteDataLength() + ImageDataLength());
    NS_ENSURE_TRUE(mImageData, NS_ERROR_OUT_OF_MEMORY);
  }

  mInitialized = PR_TRUE;
  return NS_OK;
}


/* attribute boolean mutable */
NS_IMETHODIMP gfxImageFrame::GetMutable(PRBool *aMutable)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mInitialized, "gfxImageFrame::GetMutable called on non-inited gfxImageFrame");
  *aMutable = mMutable;
  return NS_OK;
}

NS_IMETHODIMP gfxImageFrame::SetMutable(PRBool aMutable)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  mMutable = aMutable;

  if (!aMutable && mImage)
    mImage->Optimize(nsnull);

  return NS_OK;
}

/* readonly attribute PRInt32 x; */
NS_IMETHODIMP gfxImageFrame::GetX(PRInt32 *aX)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aX = mOffset.x;
  return NS_OK;
}

/* readonly attribute PRInt32 y; */
NS_IMETHODIMP gfxImageFrame::GetY(PRInt32 *aY)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aY = mOffset.y;
  return NS_OK;
}


/* readonly attribute PRInt32 width; */
NS_IMETHODIMP gfxImageFrame::GetWidth(PRInt32 *aWidth)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute PRInt32 height; */
NS_IMETHODIMP gfxImageFrame::GetHeight(PRInt32 *aHeight)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mSize.height;
  return NS_OK;
}

/* void getRect(in nsRectRef rect); */
NS_IMETHODIMP gfxImageFrame::GetRect(nsIntRect &aRect)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  aRect.SetRect(mOffset.x, mOffset.y, mSize.width, mSize.height);

  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP gfxImageFrame::GetFormat(gfx_format *aFormat)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFormat = mFormat;
  return NS_OK;
}

/* readonly attribute boolean needsBackground; */
NS_IMETHODIMP gfxImageFrame::GetNeedsBackground(PRBool *aNeedsBackground)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aNeedsBackground = (mFormat != gfxIFormats::RGB && 
                       mFormat != gfxIFormats::PAL &&
                       mFormat != gfxIFormats::BGR) ||
                      !mImage->GetIsImageComplete();
  return NS_OK;
}


/* readonly attribute unsigned long imageBytesPerRow; */
NS_IMETHODIMP gfxImageFrame::GetImageBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mImage ? mImage->GetLineStride(): mSize.width;
  return NS_OK;
}

/* readonly attribute unsigned long imageDataLength; */
NS_IMETHODIMP gfxImageFrame::GetImageDataLength(PRUint32 *aBitsLength)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = ImageDataLength();
  return NS_OK;
}

/* void getImageData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP gfxImageFrame::GetImageData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to get data on an immutable frame");

  *aData = mImage ? mImage->GetBits() : (mImageData + PaletteDataLength());
  *length = ImageDataLength();

  return NS_OK;
}

/* void getPaletteData ([array, size_is (length)] out PRUint32 palette, out unsigned long length); */
NS_IMETHODIMP gfxImageFrame::GetPaletteData(gfx_color **aPalette, PRUint32 *length)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mImageData)
    return NS_ERROR_FAILURE;

  *aPalette = (gfx_color*)mImageData;
  *length = PaletteDataLength();

  return NS_OK;
}

/* void lockImageData (); */
NS_IMETHODIMP gfxImageFrame::LockImageData()
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mImage)
    return NS_OK;
  return mImage->LockImagePixels(PR_FALSE);
}

/* void unlockImageData (); */
NS_IMETHODIMP gfxImageFrame::UnlockImageData()
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mImage)
    return NS_OK;
  return mImage->UnlockImagePixels(PR_FALSE);
}

/* attribute long timeout; */
NS_IMETHODIMP gfxImageFrame::GetTimeout(PRInt32 *aTimeout)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  // Ensure a minimal time between updates so we don't throttle the UI thread.
  // consider 0 == unspecified and make it fast but not too fast.  See bug
  // 125137, bug 139677, and bug 207059.  The behavior of recent IE and Opera
  // versions seems to be:
  // IE 6/Win:
  //   10 - 50ms go 100ms
  //   >50ms go correct speed
  // Opera 7 final/Win:
  //   10ms goes 100ms
  //   >10ms go correct speed
  // It seems that there are broken tools out there that set a 0ms or 10ms
  // timeout when they really want a "default" one.  So munge values in that
  // range.
  if (mTimeout >= 0 && mTimeout <= 10)
    *aTimeout = 100;
  else
    *aTimeout = mTimeout;

  return NS_OK;
}

NS_IMETHODIMP gfxImageFrame::SetTimeout(PRInt32 aTimeout)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  mTimeout = aTimeout;
  return NS_OK;
}

/* attribute long frameDisposalMethod; */
NS_IMETHODIMP gfxImageFrame::GetFrameDisposalMethod(PRInt32 *aFrameDisposalMethod)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFrameDisposalMethod = mDisposalMethod;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrame::SetFrameDisposalMethod(PRInt32 aFrameDisposalMethod)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  mDisposalMethod = aFrameDisposalMethod;
  return NS_OK;
}

/* attribute long blendMethod; */
NS_IMETHODIMP gfxImageFrame::GetBlendMethod(PRInt32 *aBlendMethod)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBlendMethod = mBlendMethod;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrame::SetBlendMethod(PRInt32 aBlendMethod)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  mBlendMethod = (PRInt8)aBlendMethod;
  return NS_OK;
}

NS_IMETHODIMP gfxImageFrame::GetInterface(const nsIID & aIID, void * *result)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(result);

  if (NS_SUCCEEDED(QueryInterface(aIID, result)))
    return NS_OK;
  if (mImage && aIID.Equals(NS_GET_IID(nsIImage)))
    return mImage->QueryInterface(aIID, result);

  return NS_NOINTERFACE;
}
