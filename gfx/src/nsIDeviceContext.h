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

#ifndef nsIDeviceContext_h___
#define nsIDeviceContext_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsIWidget.h"

class nsIRenderingContext;
class nsIView;
class nsIFontMetrics;
class nsIWidget;
struct nsFont;

//a cross platform way of specifying a navite device context
typedef void * nsNativeDeviceContext;

#define NS_IDEVICE_CONTEXT_IID   \
{ 0x5931c580, 0xb917, 0x11d1,    \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//a cross platform way of specifying a native palette handle
typedef void * nsPalette;

//structure used to return information about a device's palette capabilities
struct nsPaletteInfo {
  PRPackedBool  isPaletteDevice;
  PRUint8       sizePalette;  // number of entries in the palette
  PRUint8       numReserved;  // number of reserved palette entries
  nsPalette     palette;      // native palette handle
};

/**
 * Constants identifying pre-defined icons.
 * @see #LoadIcon()
 */
#define NS_ICON_LOADING_IMAGE 0
#define NS_ICON_BROKEN_IMAGE  1
#define NS_NUMBER_OF_ICONS    2

// XXX This is gross, but don't include libimg.h, because it includes ni_pixmap.h
// which includes xp_core.h which includes windows.h
struct _NI_ColorSpace;
typedef _NI_ColorSpace NI_ColorSpace;
typedef NI_ColorSpace IL_ColorSpace;

class nsIDeviceContext : public nsISupports
{
public:
  NS_IMETHOD  Init(nsNativeWidget aWidget) = 0;

  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) = 0;
  NS_IMETHOD  InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWindow) = 0;

  //these are queries to figure out how large an output unit
  //(i.e. pixel) is in terms of twips (1/20 of a point)
  NS_IMETHOD  GetDevUnitsToTwips(float &aDevUnitsToTwips) const = 0;
  NS_IMETHOD  GetTwipsToDevUnits(float &aTwipsToDevUnits) const = 0;

  //these are set by the object that created this
  //device context to define what the scale is
  //between the units used by the app and the
  //device units
  NS_IMETHOD  SetAppUnitsToDevUnits(float aAppUnits) = 0;
  NS_IMETHOD  SetDevUnitsToAppUnits(float aDevUnits) = 0;

  //these are used to query the scale values defined
  //by the above Set*() methods
  NS_IMETHOD  GetAppUnitsToDevUnits(float &aAppUnits) const = 0;
  NS_IMETHOD  GetDevUnitsToAppUnits(float &aDevUnits) const = 0;

  //returns the scrollbar dimensions in app units
  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const = 0;

  // Get the font metrics for a given font.
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics) = 0;

  //get and set the document zoom value used for display-time
  //scaling. default is 1.0 (no zoom)
  NS_IMETHOD  SetZoom(float aZoom) = 0;
  NS_IMETHOD  GetZoom(float &aZoom) const = 0;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface) = 0;

  //functions for handling gamma correction of output device
  NS_IMETHOD  GetGamma(float &aGamms) = 0;
  NS_IMETHOD  SetGamma(float aGamma) = 0;

  //XXX the return from this really needs to be ref counted somehow. MMP
  NS_IMETHOD  GetGammaTable(PRUint8 *&aGammaTable) = 0;

  NS_IMETHOD  GetNativeWidget(nsNativeWidget& aNativeWidget) = 0;

  //load the specified icon. this is a blocking call that does not return
  //until the icon is loaded.
  //release the image when you're done
  NS_IMETHOD LoadIconImage(PRInt32 aId, nsIImage*& aImage) = 0;

  /**
   * Check to see if a particular named font exists.
   * @param aFontName character string of font face name
   * @return NS_OK if font is available, else font is unavailable
   */
  NS_IMETHOD CheckFontExistence(const nsString& aFaceName) = 0;
  NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName) = 0;

  NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                              PRBool& aAliased) = 0;

  /**
   * Return the bit depth of the device.
   */
  NS_IMETHOD GetDepth(PRUint32& aDepth) = 0;

  /**
   * Return the image lib color space that's appropriate for this rendering
   * context.
   *
   * You must call IL_ReleaseColorSpace() when you're done using the color space.
   */
  NS_IMETHOD GetILColorSpace(IL_ColorSpace*& aColorSpace) = 0;

  /**
   * Returns information about the device's palette capabilities.
   */
  NS_IMETHOD GetPaletteInfo(nsPaletteInfo&) = 0;

  /**
   * Returns Platform specific pixel value for an RGB value
   */
  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel) = 0;

};

#endif /* nsIDeviceContext_h___ */
