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

#include "nsDeviceContextMac.h"
#include "nsRenderingContextMac.h"
//XXX#include "../nsGfxCIID.h"

#include "math.h"
#include "nspr.h"
#include <FixMath.h>

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

//------------------------------------------------------------------------

nsDeviceContextMac :: nsDeviceContextMac()
{
GDHandle			thegd;
PixMapHandle	thepix;
double				pix_inch;

  NS_INIT_REFCNT();
  
  // see IM Imaging with Quickdraw, chapter 5.  This is an imcomplete implementation
  thegd = ::GetMainDevice();
  HLock((Handle)thegd);
	thepix = (**thegd).gdPMap;
	pix_inch = Fix2X((**thepix).hRes);
	
	mTwipsToPixels = pix_inch/(float)NSIntPointsToTwips(72);
	mPixelsToTwips = 1.0f/mTwipsToPixels;
	
	mDepth = (**thepix).pixelSize;
	
	HUnlock((Handle)thegd);
}

//------------------------------------------------------------------------

nsDeviceContextMac :: ~nsDeviceContextMac()
{
}

//------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsDeviceContextMac, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextMac)
NS_IMPL_RELEASE(nsDeviceContextMac)

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: Init(nsNativeWidget aNativeWidget)
{
  NS_ASSERTION(!(aNativeWidget == nsnull), "attempt to init devicecontext with null widget");


	// this is a windowptr, or grafptr, native to macintosh only
	mSurface = aNativeWidget;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: GetScrollBarWidth(float &aWidth) const
{
  // XXX Should we push this to widget library
  aWidth = 240.0;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: GetScrollBarHeight(float &aHeight) const
{
  // XXX Should we push this to widget library
  aHeight = 240.0;
  return NS_OK;
}

//------------------------------------------------------------------------

nsDrawingSurface nsDeviceContextMac :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  return aContext.CreateDrawingSurface(nsnull);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::GetDepth(PRUint32& aDepth)
{
  aDepth = 24;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::CreateILColorSpace(IL_ColorSpace*& aColorSpace)
{
  nsresult result = NS_OK;


  return result;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: CheckFontExistence(const nsString& aFontName)
{
  return nsnull;
}

//------------------------------------------------------------------------


