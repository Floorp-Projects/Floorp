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

#define COLOR_CUBE_SIZE 216


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
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
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


NS_IMETHODIMP nsDeviceContextUnix :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  // XXX Should we push this to widget library
  aWidth = 240.0;
  aHeight = 240.0;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextUnix :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aSurface = aContext.CreateDrawingSurface(nsnull);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

NS_IMETHODIMP nsDeviceContextUnix :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PRUint32 newcolor = 0;

  InstallColormap();

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
  
  aColor = newcolor;
  return NS_OK;
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

   if (mSurface) {
     InstallColormap((Display*)mSurface->display, (Drawable)mSurface->drawable);
   }
   else {
    // No surface so we have to create a window just to get a drawable so we
    // can install the colormap.
     Window w;
     Display * d = XtDisplay((Widget)mWidget);
     w = ::XCreateSimpleWindow(d, 
                               RootWindow(d, DefaultScreen(d)),
                               0, 0, 1, 1, 0,
                               BlackPixel(d, DefaultScreen(d)),
                               WhitePixel(d, DefaultScreen(d)));
     InstallColormap(d, w);
     ::XDestroyWindow(d, w);
   }
}


void nsDeviceContextUnix :: InstallColormap(Display* aDisplay, Drawable aDrawable)
{

  XWindowAttributes wa;


  /* Already installed? */
  if (0 != mColormap)
    return;

  // Find the depth of this visual
  ::XGetWindowAttributes(aDisplay,
			 aDrawable,
			 &wa);
  
  mDepth = wa.depth;

  // Check to see if the colormap is writable
  mVisual = wa.visual;

  if (mVisual->c_class != TrueColor)
    mPaletteInfo.isPaletteDevice = PR_TRUE;
  else
  {
    mPaletteInfo.isPaletteDevice = PR_FALSE;
    if (mDepth == 8) {
      mPaletteInfo.numReserved = 10;
      mPaletteInfo.sizePalette = (PRUint32) pow(2, mDepth);
    }
  }

  if (mVisual->c_class == GrayScale || mVisual->c_class == PseudoColor || mVisual->c_class == DirectColor)
  {
    mWriteable = PR_TRUE;
  }
  else // We have StaticGray, StaticColor or TrueColor
    mWriteable = PR_FALSE;

  mNumCells = (PRUint32) pow(2, mDepth);
  mPaletteInfo.sizePalette = mNumCells;

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
  float       t2d;
  GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);
  Display     *dpy = XtDisplay((Widget)mWidget);
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


NS_IMETHODIMP nsDeviceContextUnix::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;
  aPaletteInfo.palette = nsnull;
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextUnix::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  InstallColormap();

  if (nsnull == mColorSpace) {

    if ((8 == mDepth) && mPaletteInfo.isPaletteDevice) {
      //
      // 8-BIT Visual
      //
      // Create a color cube. We want to use DIB_PAL_COLORS because it's faster
      // than DIB_RGB_COLORS, so make sure the indexes match that of the
      // GDI physical palette
      //
      // Note: the image library doesn't use the reserved colors, so it doesn't
      // matter what they're set to...
      IL_RGB  reserved[10];
      memset(reserved, 0, sizeof(reserved));
      IL_ColorMap* colorMap = IL_NewCubeColorMap(reserved, 10, COLOR_CUBE_SIZE + 10);
      if (nsnull == colorMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      // Create a pseudo color space
      mColorSpace = IL_CreatePseudoColorSpace(colorMap, 8, 8);
  
    } else if (16 == mDepth) {
      // 
      // 16-BIT Visual
      //
      IL_RGBBits colorRGBBits;
      // Default is to create a 16-bit color space
      colorRGBBits.red_shift = mRedOffset;  
      colorRGBBits.red_bits = mRedBits;
      colorRGBBits.green_shift = mGreenOffset;
      colorRGBBits.green_bits = mGreenBits; 
      colorRGBBits.blue_shift = mBlueOffset; 
      colorRGBBits.blue_bits = mBlueBits;  

      mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 16);
    } 
    else if (24 == mDepth) {
      DeviceContextImpl::GetILColorSpace(aColorSpace);
      return NS_OK;
    }
    else {
printf("\n\n\n\n Unknown depth %d\n",mDepth);
      IL_RGBBits colorRGBBits;
      // Default is to create a 16-bit color space
      colorRGBBits.red_shift = mRedOffset;  
      colorRGBBits.red_bits = mRedBits;
      colorRGBBits.green_shift = mGreenOffset;
      colorRGBBits.green_bits = mGreenBits; 
      colorRGBBits.blue_shift = mBlueOffset; 
      colorRGBBits.blue_bits = mBlueBits;  
      mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 16);
    }
  
    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_POSTCONDITION(nsnull != mColorSpace, "null color space");
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
  return NS_OK;
}






















