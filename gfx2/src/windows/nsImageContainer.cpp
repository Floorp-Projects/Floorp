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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageContainer.h"
#include "nsImageFrame.h"

#include "nsCOMPtr.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsImageContainer, gfxImageContainer, nsPIImageContainerWin)

nsImageContainer::nsImageContainer()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImageContainer::~nsImageContainer()
{
  /* destructor code */
}


/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP nsImageContainer::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}



/** nsPIImageContainerWin methods **/

NS_IMETHODIMP nsImageContainer::DrawImage(HDC aDestDC, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  nsresult rv;

  nsCOMPtr<gfxIImageFrame> img;
  rv = this->GetCurrentFrame(getter_AddRefs(img));

  if (NS_FAILED(rv))
    return rv;

  return NS_REINTERPRET_CAST(nsImageFrame*, img.get())->DrawImage(aDestDC, aSrcRect, aDestPoint);
}

NS_IMETHODIMP nsImageContainer::DrawScaledImage(HDC aDestDC, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsresult rv;

  nsCOMPtr<gfxIImageFrame> img;
  rv = this->GetCurrentFrame(getter_AddRefs(img));

  if (NS_FAILED(rv))
    return rv;

  return NS_REINTERPRET_CAST(nsImageFrame*, img.get())->DrawScaledImage(aDestDC, aSrcRect, aDestRect);
}

