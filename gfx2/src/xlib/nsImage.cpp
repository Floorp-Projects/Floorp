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
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void initFromDrawable (in nsIDrawable aDrawable, in gfx_coord aX, in gfx_coord aY, in gfx_dimension aWidth, in gfx_dimension aHeight); */
NS_IMETHODIMP nsImage::InitFromDrawable(nsIDrawable *aDrawable, gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setDecodedRect (in gfx_coord x1, in gfx_coord y1, in gfx_coord x2, in gfx_coord y2); */
NS_IMETHODIMP nsImage::SetDecodedRect(gfx_coord x1, gfx_coord y1, gfx_coord x2, gfx_coord y2)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_coord decodedX1; */
NS_IMETHODIMP nsImage::GetDecodedX1(gfx_coord *aDecodedX1)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_coord decodedY1; */
NS_IMETHODIMP nsImage::GetDecodedY1(gfx_coord *aDecodedY1)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_coord decodedX2; */
NS_IMETHODIMP nsImage::GetDecodedX2(gfx_coord *aDecodedX2)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_coord decodedY2; */
NS_IMETHODIMP nsImage::GetDecodedY2(gfx_coord *aDecodedY2)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint8 bits; */
NS_IMETHODIMP nsImage::GetBits(PRUint8 *aBits)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_dimension width; */
NS_IMETHODIMP nsImage::GetWidth(gfx_dimension *aWidth)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_dimension height; */
NS_IMETHODIMP nsImage::GetHeight(gfx_dimension *aHeight)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute gfx_format format; */
NS_IMETHODIMP nsImage::GetFormat(gfx_format *aFormat)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long lineStride; */
NS_IMETHODIMP nsImage::GetLineStride(PRInt32 *aLineStride)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint8 abits; */
NS_IMETHODIMP nsImage::GetAbits(PRUint8 *aAbits)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long aLineStride; */
NS_IMETHODIMP nsImage::GetALineStride(PRInt32 *aALineStride)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isRowOrderTopToBottom; */
NS_IMETHODIMP nsImage::GetIsRowOrderTopToBottom(PRBool *aIsRowOrderTopToBottom)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void setImageUpdated (in long aFlags, [const] in nsRect2 updateRect); */
NS_IMETHODIMP nsImage::SetImageUpdated(PRInt32 aFlags, const nsRect2 * updateRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
