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

#include <math.h>
#include <gtk/gtk.h>

#include "nspr.h"
#include "il_util.h"

#include "nsDeviceContextGTK.h"
#include "../nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

#define RESERVED_SIZE 0
#define COLOR_CUBE_SIZE 216 

#define NS_TO_X_RED(a)   (((NS_GET_R(a) >> (8 - mRedBits)) << mRedOffset) & mRedMask)
#define NS_TO_X_GREEN(a) (((NS_GET_G(a) >> (8 - mGreenBits)) << mGreenOffset) & mGreenMask)
#define NS_TO_X_BLUE(a)  (((NS_GET_B(a) >> (8 - mBlueBits)) << mBlueOffset) & mBlueMask)

#define NS_TO_X(a) (NS_TO_X_RED(a) | NS_TO_X_GREEN(a) | NS_TO_X_BLUE(a))

nsDeviceContextGTK :: nsDeviceContextGTK()
{
  NS_INIT_REFCNT();
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
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
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mColormap = nsnull;
  mNumCells = 0;
}

nsDeviceContextGTK :: ~nsDeviceContextGTK()
{
  if (mColormap) 
    {
      gdk_colormap_unref(mColormap);
      mColormap = nsnull;
    }
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextGTK, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextGTK)
NS_IMPL_RELEASE(nsDeviceContextGTK)

NS_IMETHODIMP nsDeviceContextGTK::Init(nsNativeWidget aNativeWidget)
{
  NS_ASSERTION(!(aNativeWidget == nsnull), "attempt to init devicecontext with null widget");
  
  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;
  
  mWidget = aNativeWidget;
  
  // ugh magic numbers here: what are 25.4 and 72 (dpi?)
  mTwipsToPixels = 
    (float)(((::gdk_screen_width()/::gdk_screen_width_mm()) * 25.4) /
            (float)NSIntPointsToTwips(72));

  mPixelsToTwips = 1.0f / mTwipsToPixels;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;
  aPaletteInfo.palette = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  InstallColormap();
  
  if ((8 == mDepth) && mPaletteInfo.isPaletteDevice) 
    {
      // First implement AllocDefaultColors
    }
  else if (16 == mDepth) 
    {
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
  else if (24 == mDepth) 
    {
      DeviceContextImpl::GetILColorSpace(aColorSpace);
      return NS_OK;
    }
  else 
    {
      DeviceContextImpl::GetILColorSpace(aColorSpace);
      return NS_OK;
    }
  
  if (nsnull == mColorSpace) 
    {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  
  NS_POSTCONDITION(nsnull != mColorSpace, "null color space");
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  // how are we going to get this? Must be set by the widget library FRV
  aWidth = 0.0;
  aHeight = 0.0;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDrawingSurface(nsIRenderingContext &aContext, 
                                                    nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;  
}

NS_IMETHODIMP nsDeviceContextGTK::ConvertPixel(nscolor aColor, 
                                               PRUint32 & aPixel)
{
  PRUint32 newcolor = 0;
  
  InstallColormap();
  
  switch (mDepth) {
    
  case 8: 
    {
      //Tedious
    }
    break;
    
  default: 
    {
      newcolor = (PRUint32)NS_TO_X(aColor);
    }
    break;
  }
  
  aPixel = newcolor;
  return NS_OK;
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextGTK::CheckFontExistence(const nsString& aFontName)
{
  return NS_OK;
}

void nsDeviceContextGTK::InstallColormap(void) 
{

  if (nsnull != mColormap)
    return;
  mDepth = ::gdk_visual_get_best_depth();
  
  GdkVisualType type = ::gdk_visual_get_best_type();  
  if (type != GDK_VISUAL_TRUE_COLOR)
    mPaletteInfo.isPaletteDevice = PR_TRUE;
  else 
    {
      mPaletteInfo.isPaletteDevice = PR_FALSE;
      if (mDepth == 8) 
        {
          mPaletteInfo.numReserved = RESERVED_SIZE;
          mPaletteInfo.sizePalette = (PRUint32) pow(2, mDepth);
        }
    }
  
  if (type == GDK_VISUAL_GRAYSCALE || 
      type == GDK_VISUAL_PSEUDO_COLOR ||
      type == GDK_VISUAL_DIRECT_COLOR) 
    {
      mWriteable = PR_TRUE;
    }
  else 
    {
      mWriteable = PR_FALSE;
    }
  
  mNumCells = (PRUint32) pow(2, mDepth);
  mPaletteInfo.sizePalette = mNumCells;
  
  // and get ourselves our colormap
  mColormap = ::gdk_colormap_new(::gdk_visual_get_best(), 0);

  // I am not entirely sure what is going on here, but I think that 
  // when the bitmap depth is 8, we want a set of default colors allocated
  // I changed the name of the method to make it a bit cleared
  if (mWriteable) 
    {
      if (mDepth == 8) 
        {
          AllocDefaultColors();
        }
    }
  GdkVisual *tmpVisual = ::gdk_visual_get_best(); //no destroy on this!

  // I sure hope this is the right way around; is red_shift mRedBits? I checked in 
  // gdk_visual_decompose_mask and I think this is correct.
  // In principle we do not set all this mask stuff, just get the visual and 
  // use these values. However, this keeps us closer to the Motif code, so it
  // easier to implement for now.
  // FRV
  mRedMask = tmpVisual->red_mask;
  mRedBits = tmpVisual->red_shift;
  mRedOffset = tmpVisual->red_prec;
  
  mGreenMask = tmpVisual->green_mask;
  mGreenBits = tmpVisual->green_shift;
  mGreenOffset = tmpVisual->green_prec;
  
  mBlueMask = tmpVisual->blue_mask;
  mBlueBits = tmpVisual->blue_shift;
  mBlueOffset = tmpVisual->blue_prec;
  
  PRUint32 i = mRedMask;
} 


void nsDeviceContextGTK :: AllocDefaultColors()
{
  // Implement this: tedious because I have > 8 bitmapdepth :-)
}





