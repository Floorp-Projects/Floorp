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

#include "nsDeviceContextPh.h"
#include "nsRenderingContextPh.h"
#include "nsDeviceContextSpecPh.h"
#include "il_util.h"


nsDeviceContextPh :: nsDeviceContextPh()
  : DeviceContextImpl()
{
}

nsDeviceContextPh :: ~nsDeviceContextPh()
{
}

NS_IMETHODIMP nsDeviceContextPh :: Init(nsNativeWidget aWidget)
{

  return DeviceContextImpl::Init(aWidget);
}

//local method...

nsresult nsDeviceContextPh :: Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext)
{
  
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: CreateRenderingContext(nsIRenderingContext *&aContext)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetCanonicalPixelScale(float &aScale) const
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{

  return NS_OK;
}

nsresult GetSysFontInfo( PhGC_t &aGC, nsSystemAttrID anID, nsFont * aFont) 
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextPh :: CheckFontExistence(const nsString& aFontName)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh::GetDepth(PRUint32& aDepth)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextPh :: GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: BeginDocument(void)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: EndDocument(void)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: BeginPage(void)
{

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: EndPage(void)
{

  return NS_OK;
}
