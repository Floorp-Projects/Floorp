/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDeviceContextGTK.h"
#include "../nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextGTK :: nsDeviceContextGTK()
{
}

nsDeviceContextGTK :: ~nsDeviceContextGTK()
{
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextGTK, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextGTK)
NS_IMPL_RELEASE(nsDeviceContextGTK)

NS_IMETHODIMP nsDeviceContextGTK::Init(nsNativeWidget aNativeWidget)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextGTK::CheckFontExistence(const nsString& aFontName)
{
  return NS_OK;
}





