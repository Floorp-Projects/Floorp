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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextUnix :: nsDeviceContextUnix()
{
  NS_INIT_REFCNT();

  mFontCache = nsnull;
  mSurface = nsnull;
}

nsDeviceContextUnix :: ~nsDeviceContextUnix()
{
  NS_IF_RELEASE(mFontCache);

  if (mSurface) delete mSurface;
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextUnix, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextUnix)
NS_IMPL_RELEASE(nsDeviceContextUnix)

nsresult nsDeviceContextUnix :: Init()
{

  return NS_OK;
}

float nsDeviceContextUnix :: GetTwipsToDevUnits() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetDevUnitsToTwips() const
{
  return 0.0;
}

void nsDeviceContextUnix :: SetAppUnitsToDevUnits(float aAppUnits)
{
}

void nsDeviceContextUnix :: SetDevUnitsToAppUnits(float aDevUnits)
{
}

float nsDeviceContextUnix :: GetAppUnitsToDevUnits() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetDevUnitsToAppUnits() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetScrollBarWidth() const
{
  return 0.0;
}

float nsDeviceContextUnix :: GetScrollBarHeight() const
{
  return 0.0;
}

nsIRenderingContext * nsDeviceContextUnix :: CreateRenderingContext(nsIView *aView)
{
  nsIRenderingContext *pContext = nsnull;
  nsIWidget *win = aView->GetWidget();
  nsresult            rv;

  mSurface = new nsDrawingSurfaceUnix();

  mSurface->display =  XtDisplay((Widget)win->GetNativeData(NS_NATIVE_WIDGET));
  mSurface->drawable = (Drawable)win->GetNativeData(NS_NATIVE_WINDOW);
  mSurface->gc       = (GC)win->GetNativeData(NS_NATIVE_GRAPHIC);

  static NS_DEFINE_IID(kRCCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRCIID, NS_IRENDERING_CONTEXT_IID);

  rv = NSRepository::CreateInstance(kRCCID, nsnull, kRCIID, (void **)&pContext);

  if (NS_OK == rv)
    InitRenderingContext(pContext, win);

  return pContext;
}

void nsDeviceContextUnix :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  aContext->Init(this, aWin);

  InstallColormap();
}

void nsDeviceContextUnix :: InstallColormap()
{

  PRUint32 screen;
  Visual * visual;
  XStandardColormap * stdcmap;
  PRInt32 numcmaps;
  Status status;
  PRInt32 i,j,k,l;
  XColor * colors;

  screen = DefaultScreen(mSurface->display);
  visual = DefaultVisual(mSurface->display, screen);

  // First, let's see if someone else did the hard work already
  status = XGetRGBColormaps(mSurface->display,
			    RootWindow(mSurface->display,screen),
			    &stdcmap, &numcmaps, XA_RGB_BEST_MAP);

  if (status == 0) {
    fprintf(stderr, "InstallColormap - FAILED\n");
    // DoSomething here!
    return;
  }

  if (stdcmap->colormap && (!stdcmap->red_max || !stdcmap->visualid)) {
    fprintf(stderr, "InstallColormap - FAILED\n");
    // DoSomething here!
    return ;
  }

  if (!stdcmap->colormap){
    // Do the hard work ... Create the Colormap here...
    XVisualInfo * vinfo, vtemplate;
    XVisualInfo * v;
    PRInt32 numinfo;

    vinfo = ::XGetVisualInfo(mSurface->display, VisualNoMask, &vtemplate, &numinfo);

    for (v = vinfo; v < vinfo + numinfo; v++)
      if (v->visualid == stdcmap->visualid){
	visual = v->visual;
	break;
      }

    stdcmap->colormap = ::XCreateColormap(mSurface->display, 
					 RootWindow(mSurface->display, screen),
					 visual, AllocAll);

    // If this is the case, we supposedly cannot change the hw colormap
    if (stdcmap->colormap == DefaultColormap(mSurface->display, screen)) {
      fprintf(stderr, "InstallColormap - FAILED\n");
      // DoSomething here!
      return;
    }

    // Go and allocate the colorcells

    
    PRUint32 numcells = stdcmap->base_pixel + 
      ((stdcmap->red_max+1)*
       (stdcmap->green_max+1)*
       (stdcmap->blue_max+1));

  }


  return ;
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
#if 0
  nsresult rv = NS_NewFontCache(&mFontCache);
  if (NS_OK != rv) {
    return rv;
  }
  mFontCache->Init(this);
#endif
  return NS_OK;
}

void nsDeviceContextUnix::FlushFontCache()
{
}


nsIFontMetrics* nsDeviceContextUnix::GetMetricsFor(const nsFont& aFont)
{
  return nsnull;
}

void nsDeviceContextUnix :: SetZoom(float aZoom)
{
}

float nsDeviceContextUnix :: GetZoom() const
{
  return 0.0;
}

nsDrawingSurface nsDeviceContextUnix :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  return ( aContext.CreateDrawingSurface(nsnull));
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

    mGammaValue = aGamma;
  }
}

PRUint8 * nsDeviceContextUnix :: GetGammaTable(void)
{
  //XXX we really need to ref count this somehow. MMP

  return nsnull;
}




