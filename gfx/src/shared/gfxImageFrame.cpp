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

NS_IMPL_ISUPPORTS2(gfxImageFrame, gfxIImageFrame, nsIInterfaceRequestor)

gfxImageFrame::gfxImageFrame() :
  mInitalized(PR_FALSE),
  mMutable(PR_TRUE),
  mHasBackgroundColor(PR_FALSE),
  mTimeout(100),
  mBackgroundColor(0),
  mDisposalMethod(0)
{
  /* member initializers and constructor code */
}

gfxImageFrame::~gfxImageFrame()
{
  /* destructor code */
}

/* void init (in nscoord aX, in nscoord aY, in nscoord aWidth, in nscoord aHeight, in gfx_format aFormat); */
NS_IMETHODIMP gfxImageFrame::Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, gfx_format aFormat,gfx_depth aDepth)
{
  if (mInitalized)
    return NS_ERROR_FAILURE;

  if (aWidth <= 0 || aHeight <= 0) {
    NS_ASSERTION(0, "error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  /* check to make sure we don't overflow a 32-bit */
  nscoord tmp = aWidth * aHeight;
  if (tmp / aHeight != aWidth) {
    NS_ASSERTION(0, "width or height too large\n");
    return NS_ERROR_FAILURE;
  }
  tmp = tmp * 4;
  if (tmp / 4 != aWidth * aHeight) {
    NS_ASSERTION(0, "width or height too large\n");
    return NS_ERROR_FAILURE;
  }

  if ( (aDepth != 8) && (aDepth != 24) ){
    NS_ERROR("This Depth is not supported\n");
    return NS_ERROR_FAILURE;
  }

  /* reject over-wide or over-tall images */
  const PRInt32 k64KLimit = 0x0000FFFF;
  if ( aWidth > k64KLimit || aHeight > k64KLimit ){
    NS_ERROR("image too big");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  mOffset.MoveTo(aX, aY);
  mSize.SizeTo(aWidth, aHeight);

  mFormat = aFormat;

  mImage = do_CreateInstance("@mozilla.org/gfx/image;1", &rv);
  NS_ASSERTION(mImage, "creation of image failed");
  if (NS_FAILED(rv)) return rv;

  gfx_depth depth = aDepth;
  nsMaskRequirements maskReq;

  switch (aFormat) {
  case gfxIFormats::BGR:
  case gfxIFormats::RGB:
    maskReq = nsMaskRequirements_kNoMask;
    break;

  case gfxIFormats::BGRA:
  case gfxIFormats::RGBA:
#ifdef DEBUG
    printf("we can't do this with the old image code\n");
#endif
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

  default:
#ifdef DEBUG
    printf("unsupposed gfx_format\n");
#endif
    break;
  }

  rv = mImage->Init(aWidth, aHeight, depth, maskReq);
  if (NS_FAILED(rv)) return rv;

  mInitalized = PR_TRUE;
  return NS_OK;
}


/* attribute boolean mutable */
NS_IMETHODIMP gfxImageFrame::GetMutable(PRBool *aMutable)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mInitalized, "gfxImageFrame::GetMutable called on non-inited gfxImageFrame");
  *aMutable = mMutable;
  return NS_OK;
}

NS_IMETHODIMP gfxImageFrame::SetMutable(PRBool aMutable)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mMutable = aMutable;

  if (!aMutable)
    mImage->Optimize(nsnull);

  return NS_OK;
}

/* readonly attribute nscoord x; */
NS_IMETHODIMP gfxImageFrame::GetX(nscoord *aX)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aX = mOffset.x;
  return NS_OK;
}

/* readonly attribute nscoord y; */
NS_IMETHODIMP gfxImageFrame::GetY(nscoord *aY)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aY = mOffset.y;
  return NS_OK;
}


/* readonly attribute nscoord width; */
NS_IMETHODIMP gfxImageFrame::GetWidth(nscoord *aWidth)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute nscoord height; */
NS_IMETHODIMP gfxImageFrame::GetHeight(nscoord *aHeight)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mSize.height;
  return NS_OK;
}

/* void getRect(in nsRectRef rect); */
NS_IMETHODIMP gfxImageFrame::GetRect(nsRect &aRect)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  aRect.SetRect(mOffset.x, mOffset.y, mSize.width, mSize.height);

  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP gfxImageFrame::GetFormat(gfx_format *aFormat)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFormat = mFormat;
  return NS_OK;
}

/* readonly attribute unsigned long imageBytesPerRow; */
NS_IMETHODIMP gfxImageFrame::GetImageBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mImage->GetLineStride();
  return NS_OK;
}

/* readonly attribute unsigned long imageDataLength; */
NS_IMETHODIMP gfxImageFrame::GetImageDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mImage->GetLineStride() * mSize.height;
  return NS_OK;
}

/* void getImageData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP gfxImageFrame::GetImageData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to get data on an immutable frame");

  *aData = mImage->GetBits();
  *length = mImage->GetLineStride() * mSize.height;

  return NS_OK;
}

/* void setImageData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP gfxImageFrame::SetImageData(const PRUint8 *aData, PRUint32 aLength, PRInt32 aOffset)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to set data on an immutable frame");
  if (!mMutable)
    return NS_ERROR_FAILURE;

  if (aLength == 0)
    return NS_OK;

  PRInt32 row_stride = mImage->GetLineStride();

  mImage->LockImagePixels(PR_FALSE);
  PRUint8 *imgData = mImage->GetBits();
  PRInt32 imgLen = row_stride * mSize.height;

  PRInt32 newOffset;
#if defined(XP_WIN) || defined(XP_OS2)
  // Adjust: We need offset to be top-down rows & LTR within each row
  //    On win32 & os/2, it's passed in as bottom-up rows & LTR within each row
  PRUint32 yOffset = ((PRUint32)(aOffset / row_stride)) * row_stride;
  newOffset = ((mSize.height - 1) * row_stride) - yOffset + (aOffset % row_stride);
#else
  newOffset = aOffset;
#endif

  if (((newOffset + (PRInt32)aLength) > imgLen) || !imgData) {
    mImage->UnlockImagePixels(PR_FALSE);
    return NS_ERROR_FAILURE;
  }

  if (aData)
    memcpy(imgData + newOffset, aData, aLength);
  else
    memset(imgData + newOffset, 0, aLength);
  mImage->UnlockImagePixels(PR_FALSE);

  PRInt32 row = (aOffset / row_stride);

  // adjust for aLength < row_stride
  PRInt32 numnewrows = ((aLength - 1) / row_stride) + 1;
  nsRect r(0, row, mSize.width, numnewrows);
  mImage->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);

  return NS_OK;
}

/* void lockImageData (); */
NS_IMETHODIMP gfxImageFrame::LockImageData()
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  return mImage->LockImagePixels(PR_FALSE);
}

/* void unlockImageData (); */
NS_IMETHODIMP gfxImageFrame::UnlockImageData()
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  return mImage->UnlockImagePixels(PR_FALSE);
}

/* readonly attribute unsigned long alphaBytesPerRow; */
NS_IMETHODIMP gfxImageFrame::GetAlphaBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mImage->GetAlphaLineStride();
  return NS_OK;
}

/* readonly attribute unsigned long alphaDataLength; */
NS_IMETHODIMP gfxImageFrame::GetAlphaDataLength(PRUint32 *aBitsLength)
{
  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mImage->GetAlphaLineStride() * mSize.height;
  return NS_OK;
}

/* void getAlphaData([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP gfxImageFrame::GetAlphaData(PRUint8 **aData, PRUint32 *length)
{
  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to get data on an immutable frame");

  *aData = mImage->GetAlphaBits();
  *length = mImage->GetAlphaLineStride() * mSize.height;

  return NS_OK;
}

/* void setAlphaData ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP gfxImageFrame::SetAlphaData(const PRUint8 *aData, PRUint32 aLength, PRInt32 aOffset)
{
  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(mMutable, "trying to set data on an immutable frame");
  if (!mMutable)
    return NS_ERROR_FAILURE;

  PRInt32 row_stride = mImage->GetAlphaLineStride();

  mImage->LockImagePixels(PR_TRUE);
  PRUint8 *alphaData = mImage->GetAlphaBits();
  PRInt32 alphaLen = row_stride * mSize.height;

  PRInt32 offset;
#if defined(XP_WIN) || defined(XP_OS2)
  // Adjust: We need offset to be top-down rows & LTR within each row
  //    On win32 & os/2, it's passed in as bottom-up rows & LTR within each row
  PRUint32 yOffset = ((PRUint32)(aOffset / row_stride)) * row_stride;
  offset = ((mSize.height - 1) * row_stride) - yOffset + (aOffset % row_stride);
#else
  offset = aOffset;
#endif

  if (((offset + (PRInt32)aLength) > alphaLen) || !alphaData) {
    mImage->UnlockImagePixels(PR_TRUE);
    return NS_ERROR_FAILURE;
  }

  if (aData)
    memcpy(alphaData + offset, aData, aLength);
  else
    memset(alphaData + offset, 0, aLength);
  mImage->UnlockImagePixels(PR_TRUE);
  return NS_OK;
}

/* void lockAlphaData (); */
NS_IMETHODIMP gfxImageFrame::LockAlphaData()
{
  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  return mImage->LockImagePixels(PR_TRUE);
}

/* void unlockAlphaData (); */
NS_IMETHODIMP gfxImageFrame::UnlockAlphaData()
{
  if (!mInitalized || !mImage->GetHasAlphaMask())
    return NS_ERROR_NOT_INITIALIZED;

  return mImage->UnlockImagePixels(PR_TRUE);
}





/* void drawTo */
NS_IMETHODIMP gfxImageFrame::DrawTo(gfxIImageFrame* aDst, nscoord aDX, nscoord aDY, nscoord aDWidth, nscoord aDHeight)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIImage> img(do_GetInterface(aDst));
  return mImage->DrawToImage(img, aDX, aDY, aDWidth, aDHeight);
}


/* attribute long timeout; */
NS_IMETHODIMP gfxImageFrame::GetTimeout(PRInt32 *aTimeout)
{
  if (!mInitalized)
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
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mTimeout = aTimeout;
  return NS_OK;
}

/* attribute long frameDisposalMethod; */
NS_IMETHODIMP gfxImageFrame::GetFrameDisposalMethod(PRInt32 *aFrameDisposalMethod)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  *aFrameDisposalMethod = mDisposalMethod;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrame::SetFrameDisposalMethod(PRInt32 aFrameDisposalMethod)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mDisposalMethod = aFrameDisposalMethod;
  return NS_OK;
}

/* attribute gfx_color backgroundColor; */
NS_IMETHODIMP gfxImageFrame::GetBackgroundColor(gfx_color *aBackgroundColor)
{
  if (!mInitalized || !mHasBackgroundColor)
    return NS_ERROR_NOT_INITIALIZED;

  *aBackgroundColor = mBackgroundColor;
  return NS_OK;
}
NS_IMETHODIMP gfxImageFrame::SetBackgroundColor(gfx_color aBackgroundColor)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  mBackgroundColor = aBackgroundColor;
  mHasBackgroundColor = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP gfxImageFrame::GetInterface(const nsIID & aIID, void * *result)
{
  if (!mInitalized)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(result);

  if (NS_SUCCEEDED(QueryInterface(aIID, result)))
    return NS_OK;
  if (mImage && aIID.Equals(NS_GET_IID(nsIImage)))
    return mImage->QueryInterface(aIID, result);

  return NS_NOINTERFACE;
}
 
