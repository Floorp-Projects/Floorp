/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDeviceContextMotif.h"
#include "nsRenderingContextMotif.h"
#include "nsGfxCIID.h"

#include "math.h"
#include "nspr.h"
#include "il_util.h"


#define NS_TO_X_COMPONENT(a) ((a << 8) | (a))

#define NS_TO_X_RED(a)   (((NS_GET_R(a) >> (8 - mRedBits)) << mRedOffset) & mRedMask)
#define NS_TO_X_GREEN(a) (((NS_GET_G(a) >> (8 - mGreenBits)) << mGreenOffset) & mGreenMask)
#define NS_TO_X_BLUE(a)  (((NS_GET_B(a) >> (8 - mBlueBits)) << mBlueOffset) & mBlueMask)

#define NS_TO_X(a) (NS_TO_X_RED(a) | NS_TO_X_GREEN(a) | NS_TO_X_BLUE(a))

#define COLOR_CUBE_SIZE 216 

typedef struct {
   int red, green, blue;
} nsReservedColor;

#define RESERVED_SIZE 0

nsReservedColor sReservedColors[] = {  
                     { 0,     0,   0 },
                     { 128,   0,   0 },
                     {  0,  128,   0 }, 
                     { 128, 128,   0 }, 
                     { 0,     0, 128 }, 
                     {128,    0, 128 }, 
                     { 0,   128, 128 }, 
                     { 192, 192, 192 }, 
                     { 192, 220, 192 }, 
                     { 166, 202, 240 } };


nsDeviceContextMotif :: nsDeviceContextMotif()
{
  NS_INIT_REFCNT();
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
  mColorsAllocated = PR_FALSE;
  mIndex = 0;
  mDeviceColors = NULL;
  mDisplay = 0;
}

nsDeviceContextMotif :: ~nsDeviceContextMotif()
{
  if (mSurface) {
    delete mSurface;
    mSurface = nsnull;
  }

  if (mIndex)
    delete mIndex;

  if (mDeviceColors)
    delete mDeviceColors;
}

NS_IMPL_ISUPPORTS1(nsDeviceContextMotif, nsIDeviceContext)

NS_IMETHODIMP nsDeviceContextMotif :: Init(nsNativeWidget aNativeWidget)
{
  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;

  // XXX We really need to have Display passed to us since it could be specified
  //     not from the environment, which is the one we use here.

  mWidget = aNativeWidget;

  if (nsnull != mWidget)
  {
    Display *display = XtDisplay((Widget)mWidget);
    Screen *screen = DefaultScreen(display);
    mTwipsToPixels = (((float)::XDisplayWidth(display, screen)) /
  		    ((float)::XDisplayWidthMM(display ,screen )) * 25.4) / 
      (float)NSIntPointsToTwips(72);
    
    mPixelsToTwips = 1.0f / mTwipsToPixels;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextMotif :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  aSupportsWidgets = PR_TRUE;

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextMotif :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  // XXX Should we push this to widget library
  aWidth = 240.0;
  aHeight = 240.0;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

Display *nsDeviceContextMotif::GetDisplay()
{
  if (mDisplay)
    return(mDisplay);

  if (mSurface) { 
    mDisplay = mSurface->display;
  }
  else { 
    mDisplay = XtDisplay((Widget)mWidget);
  }

  return(mDisplay);
}

// Try to find existing color then allocate color if one can not be found and
// aCanAlloc is PR_TRUE. If all else fails try and find the closest match to
// an existing color. 

uint8 nsDeviceContextMotif :: AllocColor(uint8 aRed, uint8 aGreen, uint8 aBlue, PRBool aCanAlloc)
{
  Display* display = GetDisplay();

  if (NULL == mDeviceColors) {
    mDeviceColors = new XColor[256];
    for (int i = 0; i < 256; i++) {
      mDeviceColors[i].pixel = i;
    }
  }

  XQueryColors(display, mColormap,  mDeviceColors, 256);

   // Look for perfect match
  for (int i = 0; i < 256; i++) {
    if (((mDeviceColors[i].red >> 8)== aRed) && 
        ((mDeviceColors[i].green >> 8) == aGreen) &&
        ((mDeviceColors[i].blue >> 8) == aBlue)) {
      return(mDeviceColors[i].pixel);
    }
  }

  // Try and allocate the color 
  XColor color;
  color.red = aRed << 8;
  color.green = aGreen << 8;
  color.blue = aBlue << 8;
  color.pixel = 0;
  color.flags = DoRed | DoGreen | DoBlue;
  color.pad = 0;

  if (::XAllocColor(display, mColormap, &color)) { 
    return(color.pixel);
  }

  // No color found so look for the closest match

  uint8 closest = 0;
  uint8 r, g, b;
  unsigned long distance = ~0;
  unsigned long d;
  int dr, dg, db;
  for (int colorindex = 0; colorindex < 256; colorindex++) {
    r = mDeviceColors[colorindex].red >> 8;
    g = mDeviceColors[colorindex].green >> 8;
    b = mDeviceColors[colorindex].blue >> 8;
    dr = r - aRed; 
    dg = g - aGreen; 
    db = b - aBlue; 
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    d = (dr << 1) + (dg << 2) + db;
    if (d < distance) {
     distance = d;
     closest = mDeviceColors[colorindex].pixel;
    }
  }

  return(closest);
}


NS_IMETHODIMP nsDeviceContextMotif :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PRUint32 newcolor = 0;

  InstallColormap();

  /*
    For now, we assume anything in 12 planes or more is a TrueColor visual. 
    If it is not (like older IRIS GL graphics boards, we'll look stupid for now.
    */

  switch (mDepth) {
    
  case 8: {
      newcolor = AllocColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), PR_TRUE);

  } 
  break;

  default: {
      newcolor = (PRUint32)NS_TO_X(aColor);
  } 
  break;
    
  } 
  
  aPixel = newcolor;
  return NS_OK;
}


void nsDeviceContextMotif :: InstallColormap()
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


void nsDeviceContextMotif :: InstallColormap(Display* aDisplay, Drawable aDrawable)
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
      mPaletteInfo.numReserved = RESERVED_SIZE;
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
       AllocColors();
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

nsDrawingSurface nsDeviceContextMotif :: GetDrawingSurface()
{
  return mSurface;
}



NS_IMETHODIMP nsDeviceContextMotif :: CheckFontExistence(const nsString& aFontName)
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

void nsDeviceContextMotif::AllocColors()
{
    uint8* inx = new uint8[256];

    if (PR_TRUE == mColorsAllocated)
      return;
 
    mColorsAllocated = PR_TRUE;

    Display* d;
    if (mSurface) { 
      d = mSurface->display;
    }
    else {
      d = XtDisplay((Widget)mWidget);
    }

#if 0
    IL_RGB  reserved[RESERVED_SIZE];
    //memset(reserved, 0, sizeof(reserved));
    // Setup the reserved colors
    for (int i = 0; i < RESERVED_SIZE; i++) {
     reserved[i].red = sReservedColors[i].red;
     reserved[i].green = sReservedColors[i].green;
     reserved[i].blue = sReservedColors[i].blue;
     inx[i] = i;
    }
#else
    IL_RGB  reserved[1]; //XXX REMOVE THIS here and below
#endif

    IL_ColorMap* colorMap = IL_NewCubeColorMap(reserved, RESERVED_SIZE, COLOR_CUBE_SIZE + RESERVED_SIZE);

      // Create a pseudo color space
    IL_ColorSpace* colorSpace = IL_CreatePseudoColorSpace(colorMap, 8, 8);

      // Create a logical palette
     NI_RGB* map = colorSpace->cmap.map;    

     for (PRInt32 colorindex = RESERVED_SIZE; colorindex < (COLOR_CUBE_SIZE + RESERVED_SIZE); colorindex++)     {
       inx[colorindex] = AllocColor(map->red, map->green, map->blue, PR_TRUE);
       map++;
     }

    mIndex = inx;

    if (mColorSpace)
      mColorSpace->cmap.index = mIndex;

}

NS_IMETHODIMP nsDeviceContextMotif::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;
  aPaletteInfo.palette = nsnull;
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextMotif::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  InstallColormap();

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
//      IL_ColorMap* colorMap = IL_NewCubeColorMap(reserved, 10, COLOR_CUBE_SIZE + 10);
      IL_ColorMap* colorMap = IL_NewCubeColorMap(reserved, 0, COLOR_CUBE_SIZE );

      if (nsnull == colorMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      // Create a pseudo color space
      mColorSpace = IL_CreatePseudoColorSpace(colorMap, 8, 8);
      mColorSpace->cmap.index = mIndex;

  
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
      DeviceContextImpl::GetILColorSpace(aColorSpace);
      return NS_OK;
    }
  
    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }

  NS_POSTCONDITION(nsnull != mColorSpace, "null color space");
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  aWidth = 1;
  aHeight = 1;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextMotif::GetClientRect(nsRect &aRect)
{
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = 0;
  aRect.height = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextMotif::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextMotif::BeginDocument(PRUnichar * aTitle)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif::EndDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif::BeginPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif::EndPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMotif::GetSystemAttribute(nsSystemAttrID anID, 
                                                       SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_WidgetBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    case eSystemAttr_Color_TextSelectBackground:
      *aInfo->mColor = NS_RGB(255,255,255);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = NS_RGB(0,0,0);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight:
        aInfo->mSize = 20;
        break;
    case eSystemAttr_Size_ScrollbarWidth: 
        aInfo->mSize = 20;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
      //      aInfo->mSize = style->klass->xthickness;
      aInfo->mSize = 1;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
      //        aInfo->mSize = style->klass->ythickness;
        aInfo->mSize = 1;
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = 4;
        break;
    //---------
    // Fonts
    //---------
    case eSystemAttr_Font_Caption:
    case eSystemAttr_Font_Icon:
    case eSystemAttr_Font_Menu:
    case eSystemAttr_Font_MessageBox:
    case eSystemAttr_Font_SmallCaption:
    case eSystemAttr_Font_StatusBar:
    case eSystemAttr_Font_Tooltips:
    case eSystemAttr_Font_Widget:
      status = NS_ERROR_FAILURE;
      break;
  } // switch

  return NS_OK;
}
