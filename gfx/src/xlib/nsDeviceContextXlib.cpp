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

#include "nsRenderingContextXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "nsDeviceContextXlib.h"

nsDeviceContextXlib::nsDeviceContextXlib()
  : DeviceContextImpl()
{
  printf("nsDeviceContextXlib::nsDeviceContextXlib()\n");
  NS_INIT_REFCNT();
}

NS_IMETHODIMP nsDeviceContextXlib::Init(nsNativeWidget aNativeWidget)
{
  printf("nsDeviceContextXlib::Init()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  printf("nsDeviceContextXlib::CreateRenderingContext()\n");

  nsIRenderingContext *context = nsnull;
  nsIDrawingSurface   *surface = nsnull;
  // XXX umm...this isn't done.
  context = new nsRenderingContextXlib();

  if (nsnull != context) {
    NS_ADDREF(context);
    surface = new nsDrawingSurfaceXlib();
  }

  aContext = context;
  
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  printf("nsDeviceContextXlib::SupportsNativeWidgets()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  printf("nsDeviceContextXlib::GetScrollBarDimensions()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  printf("nsDeviceContextXlib::GetSystemAttribute()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  printf("nsDeviceContextXlib::GetDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  printf("nsDeviceContextXlib::ConvertPixel()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::CheckFontExistence(const nsString& aFontName)
{
  printf("nsDeviceContextXlib::CheckFontExistence()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  printf("nsDeviceContextXlib::GetDeviceSurfaceDimensions()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  printf("nsDeviceContextXlib::GetDeviceContextFor()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginDocument(void)
{
  printf("nsDeviceContextXlib::BeginDocument()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::EndDocument(void)
{
  printf("nsDeviceContextXlib::EndDocument()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::BeginPage(void)
{
  printf("nsDeviceContextXlib::BeginPage()\n");
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXlib::EndPage(void)
{
  printf("nsDeviceContextXlib::EndPage()\n");
  return NS_OK;
}
