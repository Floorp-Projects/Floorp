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

#include "nsDeviceContextUnix.h"
#include "nsRenderingContextUnix.h"
#include "../nsGfxCIID.h"

#include "math.h"
#include "nspr.h"

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

#define NS_TO_X_COMPONENT(a) ((a << 8) | (a))

nsDeviceContextUnix :: nsDeviceContextUnix()
{
  NS_INIT_REFCNT();

  mFontCache = nsnull;
  mSurface = nsnull;

  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;

  mDevUnitsToAppUnits = 1.0f;
  mAppUnitsToDevUnits = 1.0f;

  mGammaValue = 1.0f;
  mGammaTable = new PRUint8[256];

  mZoom = 1.0f;

}

nsDeviceContextUnix :: ~nsDeviceContextUnix()
{

  if (nsnull != mGammaTable)
  {
    delete mGammaTable;
    mGammaTable = nsnull;
  }

  NS_IF_RELEASE(mFontCache);

  if (mSurface) delete mSurface;
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextUnix, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextUnix)
NS_IMPL_RELEASE(nsDeviceContextUnix)

nsresult nsDeviceContextUnix :: Init()
{
  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;

  return NS_OK;
}

float nsDeviceContextUnix :: GetTwipsToDevUnits() const
{
  return mTwipsToPixels;
}

float nsDeviceContextUnix :: GetDevUnitsToTwips() const
{
  return mPixelsToTwips;
}


void nsDeviceContextUnix :: SetAppUnitsToDevUnits(float aAppUnits)
{
  mAppUnitsToDevUnits = aAppUnits;
}

void nsDeviceContextUnix :: SetDevUnitsToAppUnits(float aDevUnits)
{
  mDevUnitsToAppUnits = aDevUnits;
}

float nsDeviceContextUnix :: GetAppUnitsToDevUnits() const
{
  return mAppUnitsToDevUnits;
}

float nsDeviceContextUnix :: GetDevUnitsToAppUnits() const
{
  return mDevUnitsToAppUnits;
}

float nsDeviceContextUnix :: GetScrollBarWidth() const
{
  // XXX Should we push this to widget library
  return 240.0;
}

float nsDeviceContextUnix :: GetScrollBarHeight() const
{
  // XXX Should we push this to widget library
  return 240.0;
}

nsIRenderingContext * nsDeviceContextUnix :: CreateRenderingContext(nsIView *aView)
{
  nsIRenderingContext *pContext = nsnull;
  nsIWidget *win = aView->GetWidget();
  nsresult            rv;

  mSurface = new nsDrawingSurfaceUnix();

  mSurface->display =  (Display *)win->GetNativeData(NS_NATIVE_DISPLAY);
  mSurface->drawable = (Drawable)win->GetNativeData(NS_NATIVE_WINDOW);
  mSurface->gc       = (GC)win->GetNativeData(NS_NATIVE_GRAPHIC);

  static NS_DEFINE_IID(kRCCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRCIID, NS_IRENDERING_CONTEXT_IID);

  rv = NSRepository::CreateInstance(kRCCID, nsnull, kRCIID, (void **)&pContext);

  if (NS_OK == rv)
    InitRenderingContext(pContext, win);

  NS_IF_RELEASE(win);
  return pContext;
}

void nsDeviceContextUnix :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  aContext->Init(this, aWin);

  mTwipsToPixels = (((float)::XDisplayWidth(mSurface->display, DefaultScreen(mSurface->display))) /
		    ((float)::XDisplayWidthMM(mSurface->display,DefaultScreen(mSurface->display) )) * 25.4) / 
    NS_POINTS_TO_TWIPS_FLOAT(72.0f);

  mPixelsToTwips = 1.0f / mTwipsToPixels;

  //InstallColormap();
}

PRUint32 nsDeviceContextUnix :: ConvertPixel(nscolor aColor)
{
  PRUint32 newcolor = 0;

  if (mDepth == 8) {

    if (mWriteable == PR_FALSE) {

      Status rc ;
      XColor colorcell;
      
      colorcell.red   = NS_TO_X_COMPONENT(NS_GET_R(aColor));
      colorcell.green = NS_TO_X_COMPONENT(NS_GET_G(aColor));
      colorcell.blue  = NS_TO_X_COMPONENT(NS_GET_B(aColor));
      
      colorcell.pixel = 0;
      colorcell.flags = 0;
      colorcell.pad = 0;

      // On static displays, this will return closest match
      rc = ::XAllocColor(mSurface->display,
			 mColormap,
			 &colorcell);

      if (rc == 0) {
	// Punt ... this cannot happen!
	fprintf(stderr,"WHOA! IT FAILED!\n");
      } else {
	newcolor = colorcell.pixel;
      }
    } else {

      // Check to see if this exact color is present.  If not, add it ourselves.  
      // If there are no unallocated cells left, do our own closest match lookup 
      //since X doesn't provide us with one.
      
      Status rc ;
      XColor colorcell;
      
      colorcell.red   = NS_TO_X_COMPONENT(NS_GET_R(aColor));
      colorcell.green = NS_TO_X_COMPONENT(NS_GET_G(aColor));
      colorcell.blue  = NS_TO_X_COMPONENT(NS_GET_B(aColor));
      
      colorcell.pixel = 0;
      colorcell.flags = 0;
      colorcell.pad = 0;

      // On non-static displays, this may fail
      rc = ::XAllocColor(mSurface->display,
			 mColormap,
			 &colorcell);

      if (rc == 0) {
	
	// The color does not already exist AND we do not have any unallocated colorcells left
	// At his point we need to implement our own lookup matching algorithm.

	unsigned long pixel;
      
	rc = ::XAllocColorCells(mSurface->display,
				mColormap,
				False,0,0,
				&pixel,
				1);
	
	if (rc == 0){

	  fprintf(stderr, "Failed to allocate Color cells...this sux\n");

	} else {

	  colorcell.pixel = pixel;
	  colorcell.pad = 0 ;
	  colorcell.flags = DoRed | DoGreen | DoBlue ;
	  colorcell.red   = NS_TO_X_COMPONENT(NS_GET_R(aColor));
	  colorcell.green = NS_TO_X_COMPONENT(NS_GET_G(aColor));
	  colorcell.blue  = NS_TO_X_COMPONENT(NS_GET_B(aColor));
	  
	  ::XStoreColor(mSurface->display, mColormap, &colorcell);
	  
	  newcolor = colorcell.pixel;
	
	} 
      } else {
	newcolor = colorcell.pixel;
      }
    } 

  }

  if (mDepth == 24) {
    newcolor = aColor & 0x00ffffff;
  }
  
  return (newcolor);
}

void nsDeviceContextUnix :: InstallColormap()
{

  /*  
      Unfortunately, we don't have control of the visual created for this display.  
      That should be managed at an application level, since the gfx only cares that all 
      values be passed in as 32 bit RGBA quantites.  

      This means we have to write lots and lots of code to support the fact that any 
      number of visuals may be the one associated with this device context.
   */

  XWindowAttributes wa;

  // Find the depth of this visual
  ::XGetWindowAttributes(mSurface->display,
			 mSurface->drawable,
			 &wa);
  
  mDepth = wa.depth;

  // Check to see if the colormap is writable
  mVisual = wa.visual;

  if (mVisual->c_class == GrayScale || mVisual->c_class == PseudoColor || mVisual->c_class == DirectColor)
    mWriteable = PR_TRUE;
  else // We have StaticGray, StaticColor or TrueColor
    mWriteable = PR_FALSE;

  mNumCells = pow(2, mDepth);

  mColormap = wa.colormap;

  // if the colormap is writeable .....
  if (mWriteable) {

    // XXX We should check the XExtensions to see if this hardware supports multiple
    //         hardware colormaps.  If so, change this colormap to be a RGB ramp.
    if (mDepth == 8) {

    }
  }

}

nsIFontCache* nsDeviceContextUnix::GetFontCache()
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  NS_ADDREF(mFontCache);
  return mFontCache;
}

nsresult nsDeviceContextUnix::CreateFontCache()
{
  nsresult rv = NS_NewFontCache(&mFontCache);
  if (NS_OK != rv) {
    return rv;
  }
  mFontCache->Init(this);
  return NS_OK;
}

void nsDeviceContextUnix::FlushFontCache()
{
  NS_RELEASE(mFontCache);
}


nsIFontMetrics* nsDeviceContextUnix::GetMetricsFor(const nsFont& aFont)
{
  if (nsnull == mFontCache) {
    if (NS_OK != CreateFontCache()) {
      return nsnull;
    }
  }
  return mFontCache->GetMetricsFor(aFont);
}

void nsDeviceContextUnix :: SetZoom(float aZoom)
{
  mZoom = aZoom;
}

float nsDeviceContextUnix :: GetZoom() const
{
  return mZoom;
}

nsDrawingSurface nsDeviceContextUnix :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  return ( aContext.CreateDrawingSurface(nsnull));
}

nsDrawingSurface nsDeviceContextUnix :: GetDrawingSurface()
{
  return (mSurface);
}

float nsDeviceContextUnix :: GetGamma(void)
{
  return mGammaValue;
}

void nsDeviceContextUnix :: SetGamma(float aGamma)
{
  if (aGamma != mGammaValue)
  {
    //we don't need to-recorrect existing images for this case
    //so pass in 1.0 for the current gamma regardless of what it
    //really happens to be. existing images will get a one time
    //re-correction when they're rendered the next time. MMP

    SetGammaTable(mGammaTable, 1.0f, aGamma);

    mGammaValue = aGamma;
  }
}

PRUint8 * nsDeviceContextUnix :: GetGammaTable(void)
{
  //XXX we really need to ref count this somehow. MMP

  return mGammaTable;
}

void nsDeviceContextUnix :: SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma)
{
  double fgval = (1.0f / aCurrentGamma) * (1.0f / aNewGamma);

  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    aTable[cnt] = (PRUint8)(pow((double)cnt * (1. / 256.), fgval) * 255.99999999);
}








