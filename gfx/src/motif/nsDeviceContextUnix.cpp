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
#include "il_util.h"

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

#define NS_TO_X_COMPONENT(a) ((a << 8) | (a))

#define NS_TO_X_RED(a)   (((NS_GET_R(a) >> (8 - mRedBits)) << mRedOffset) & mRedMask)
#define NS_TO_X_GREEN(a) (((NS_GET_G(a) >> (8 - mGreenBits)) << mGreenOffset) & mGreenMask)
#define NS_TO_X_BLUE(a)  (((NS_GET_B(a) >> (8 - mBlueBits)) << mBlueOffset) & mBlueMask)

#define NS_TO_X(a) (NS_TO_X_RED(a) | NS_TO_X_GREEN(a) | NS_TO_X_BLUE(a))


nsDeviceContextUnix :: nsDeviceContextUnix()
{
  mSurface = nsnull;
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mVisual = nsnull;
  mRedMask = 0;
  mGreenMask = 0;
  mBlueMask = 0;
  mRedBits = 0;
  mGreenBits = 0;
  mBlueBits = 0;
  mRedOffset = 0;
  mGreenOffset = 0;
  mBlueOffset = 0;
  mDepth = 0 ;
  mColormap = 0 ;
}

nsDeviceContextUnix :: ~nsDeviceContextUnix()
{
  if (mSurface) {
    delete mSurface;
    mSurface = nsnull;
  }
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextUnix, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextUnix)
NS_IMPL_RELEASE(nsDeviceContextUnix)

NS_IMETHODIMP nsDeviceContextUnix :: Init(nsNativeWidget aNativeWidget)
{
  NS_ASSERTION(!(aNativeWidget == nsnull), "attempt to init devicecontext with null widget");

  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;

  // XXX We really need to have Display passed to us since it could be specified
  //     not from the environment, which is the one we use here.

  mWidget = aNativeWidget;

  if (nsnull != mWidget)
  {
    mTwipsToPixels = (((float)::XDisplayWidth(XtDisplay((Widget)mWidget), DefaultScreen(XtDisplay((Widget)mWidget)))) /
  		    ((float)::XDisplayWidthMM(XtDisplay((Widget)mWidget),DefaultScreen(XtDisplay((Widget)mWidget)) )) * 25.4) / 
      (float)NSIntPointsToTwips(72);
    
    mPixelsToTwips = 1.0f / mTwipsToPixels;
  }

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextUnix :: GetScrollBarWidth(float &aWidth) const
{
  // XXX Should we push this to widget library
  aWidth = 240.0;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextUnix :: GetScrollBarHeight(float &aHeight) const
{
  // XXX Should we push this to widget library
  aHeight = 240.0;
  return NS_OK;
}


nsDrawingSurface nsDeviceContextUnix :: GetDrawingSurface(nsIRenderingContext &aContext)
{
  return aContext.CreateDrawingSurface(nsnull);
}

PRUint32 nsDeviceContextUnix :: ConvertPixel(nscolor aColor)
{
  PRUint32 newcolor = 0;

  /*
    For now, we assume anything in 12 planes or more is a TrueColor visual. 
    If it is not (like older IRIS GL graphics boards, we'll look stupid for now.
    */

  switch (mDepth) {
    
  case 8:
    {
      
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
	} // rc == 0
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
	    
	  } // rc == 0 
	} else {
	  newcolor = colorcell.pixel;
	} // rc == 0
      } // mWriteable == FALSE
    } // 8
  break;

  case 12:
    {
      newcolor = (PRUint32)NS_TO_X(aColor);
    } // 12
  break;

  case 15:
    {
      newcolor = (PRUint32)NS_TO_X(aColor);
    } // 15
  break;

  case 16:
    {
      newcolor = (PRUint32)NS_TO_X(aColor);
    } // 16
  break;

  case 24:
    {
      newcolor = (PRUint32)NS_TO_X(aColor);
	//newcolor = (PRUint32)NS_TO_X24(aColor);
    } // 24
  break;
  
  default:
    {
      newcolor = (PRUint32)NS_TO_X(aColor);
      //      newcolor = (PRUint32) aColor;
    } // default
  break;
    
  } // switch(mDepth)
  
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


  /* Already installed? */
  if (0 != mColormap)
    return;

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

  // Compute rgb masks and number of bits for each
  mRedMask = mVisual->red_mask;
  mGreenMask = mVisual->green_mask;
  mBlueMask = mVisual->blue_mask;

  PRUint32 i = mRedMask;

  while (i) {
    
    if ((i & 0x1) != 0) {
      mRedBits++;
    } else {
      mRedOffset++;
    }

    i = i >> 1;

  }

  i = mGreenMask;

  while (i) {
    
    if ((i & 0x1) != 0)
      mGreenBits++;
    else
      mGreenOffset++;

    i = i >> 1;

  }

  i = mBlueMask;

  while (i) {
    
    if ((i & 0x1) != 0)
      mBlueBits++;
    else
      mBlueOffset++;

    i = i >> 1;

  }
  
}

nsDrawingSurface nsDeviceContextUnix :: GetDrawingSurface()
{
  return mSurface;
}



NS_IMETHODIMP nsDeviceContextUnix :: CheckFontExistence(const nsString& aFontName)
{
  char        **fnames = nsnull;
  PRInt32     namelen = aFontName.Length() + 1;
  char        *wildstring = (char *)PR_Malloc(namelen + 200);
  PRInt32     dpi = NSToIntRound(GetTwipsToDevUnits() * 1440);
  Display     *dpy = XtDisplay((Widget)GetNativeWidget());
  int         numnames = 0;
  XFontStruct *fonts;
  nsresult    rv = NS_ERROR_FAILURE;

  if (nsnull == wildstring)
    return NS_ERROR_UNEXPECTED;

  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;

  char* fontName = aFontName.ToNewCString();
  PR_snprintf(wildstring, namelen + 200,
             "*-%s-*-*-normal--*-*-%d-%d-*-*-*",
             fontName, dpi, dpi);
  delete [] fontName;

  fnames = ::XListFontsWithInfo(dpy, wildstring, 1, &numnames, &fonts);

  if (numnames > 0)
  {
    ::XFreeFontInfo(fnames, fonts, numnames);
    rv = NS_OK;
  }

  PR_Free(wildstring);

  return rv;
}























