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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImage.h"

NS_IMPL_ISUPPORTS1(nsImage, nsIImage)

nsImage::nsImage()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImage::~nsImage()
{
  /* destructor code */
}

/* void init (in gfx_dimension aWidth, in gfx_dimension aHeight, in gfx_format aFormat); */
NS_IMETHODIMP nsImage::Init(gfx_dimension aWidth, gfx_dimension aHeight, gfx_format aFormat)
{
  mSize.SizeTo(aWidth, aHeight);
  mFormat = aFormat;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void initFromDrawable (in nsIDrawable aDrawable, in gfx_coord aX, in gfx_coord aY, in gfx_dimension aWidth, in gfx_dimension aHeight); */
NS_IMETHODIMP nsImage::InitFromDrawable(nsIDrawable *aDrawable, gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_dimension width; */
NS_IMETHODIMP nsImage::GetWidth(gfx_dimension *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute gfx_dimension height; */
NS_IMETHODIMP nsImage::GetHeight(gfx_dimension *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP nsImage::GetFormat(gfx_format *aFormat)
{
  *aFormat = mFormat;
  return NS_OK;
}

/* PRUint8 getBits (); */
NS_IMETHODIMP nsImage::GetBits(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setBits ([array, size_is (length), const] in PRUint8 data, in unsigned long length, in long offset); */
NS_IMETHODIMP nsImage::SetBits(const PRUint8 *data, PRUint32 length, PRInt32 offset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
