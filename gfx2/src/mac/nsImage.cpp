/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImage.h"

#include "nsUnitConverters.h"

NS_IMPL_ISUPPORTS1(nsImage, nsIImage2)

nsImage::nsImage() :
  mBits(nsnull)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImage::~nsImage()
{
  /* destructor code */
  delete[] mBits;
  mBits = nsnull;
}

/* void init (in gfx_dimension aWidth, in gfx_dimension aHeight, in gfx_format aFormat); */
NS_IMETHODIMP nsImage::Init(gfx_dimension aWidth, gfx_dimension aHeight, gfx_format aFormat)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  delete[] mBits;

  mSize.SizeTo(aWidth, aHeight);
  mFormat = aFormat;

  switch (aFormat) {
  case nsIGFXFormat::RGB:
  case nsIGFXFormat::RGB_A1:
  case nsIGFXFormat::RGB_A8:
    mDepth = 24;
    break;
  case nsIGFXFormat::RGBA:
    mDepth = 32;
    break;
  default:
    printf("unsupposed gfx_format\n");
    break;
  }

  PRInt32 ceilWidth(GFXCoordToIntCeil(mSize.width));

  mBytesPerRow = (ceilWidth * mDepth) >> 5;

  if ((ceilWidth * mDepth) & 0x1F)
    mBytesPerRow++;
  mBytesPerRow <<= 2;

  mBitsLength = mBytesPerRow * GFXCoordToIntCeil(mSize.height);

  mBits = new PRUint8[mBitsLength];

  return NS_OK;
}

/* void initFromDrawable (in nsIDrawable aDrawable, in gfx_coord aX, in gfx_coord aY, in gfx_dimension aWidth, in gfx_dimension aHeight); */
NS_IMETHODIMP nsImage::InitFromDrawable(nsIDrawable *aDrawable, gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_dimension width; */
NS_IMETHODIMP nsImage::GetWidth(gfx_dimension *aWidth)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute gfx_dimension height; */
NS_IMETHODIMP nsImage::GetHeight(gfx_dimension *aHeight)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aHeight = mSize.height;
  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP nsImage::GetFormat(gfx_format *aFormat)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aFormat = mFormat;
  return NS_OK;
}

/* readonly attribute unsigned long bytesPerRow; */
NS_IMETHODIMP nsImage::GetBytesPerRow(PRUint32 *aBytesPerRow)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aBytesPerRow = mBytesPerRow;
  return NS_OK;
}

/* readonly attribute unsigned long bitsLength; */
NS_IMETHODIMP nsImage::GetBitsLength(PRUint32 *aBitsLength)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aBitsLength = mBitsLength;
  return NS_OK;
}

/* void getBits([array, size_is(length)] out PRUint8 bits, out unsigned long length); */
NS_IMETHODIMP nsImage::GetBits(PRUint8 **aBits, PRUint32 *length)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  *aBits = mBits;
  *length = mBitsLength;

  return NS_OK;
}

/* void setBits ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImage::SetBits(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  if (!mBits)
    return NS_ERROR_NOT_INITIALIZED;

  if (((PRUint32)offset + length) > mBitsLength)
    return NS_ERROR_FAILURE;

  memcpy(mBits + offset, data, length);

  return NS_OK;
}
