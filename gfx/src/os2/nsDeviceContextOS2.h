/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nsDeviceContextOS2_h
#define _nsDeviceContextOS2_h

#include "nsDeviceContext.h"
#include "nsRenderingContextOS2.h"

// Device contexts are either for windows or for printers.
// If the former, the DeviceContextImpl member mWidget is set.
// If the latter, the mDC member here is set.
// (yes, I know: eventually want to split these guys up)

class nsIPaletteOS2;
class nsDrawingSurfaceOS2;

class nsDeviceContextOS2 : public DeviceContextImpl
{
 public:
   nsDeviceContextOS2();

   NS_IMETHOD Init( nsNativeWidget aWidget);

   // what a bizarre method.  These floats are app units.
   NS_IMETHOD GetScrollBarDimensions( float &aWidth, float &aHeight) const;

   NS_IMETHOD GetSystemAttribute( nsSystemAttrID anID, SystemAttrStruct *aInfo) const;

   // get a low level drawing surface for rendering. the rendering context
   // that is passed in is used to create the drawing surface if there isn't
   // already one in the device context. the drawing surface is then cached
   // in the device context for re-use.
   NS_IMETHOD GetDrawingSurface( nsIRenderingContext &aContext,
                                 nsDrawingSurface &aSurface);
   NS_IMETHOD CheckFontExistence( const nsString &aFontName);
   NS_IMETHOD GetDepth( PRUint32 &aDepth);
   NS_IMETHOD GetILColorSpace( IL_ColorSpace*& aColorSpace);
   NS_IMETHOD GetPaletteInfo( nsPaletteInfo &);
   NS_IMETHOD ConvertPixel( nscolor aColor, PRUint32 & aPixel);
   NS_IMETHOD GetDeviceSurfaceDimensions( PRInt32 &aWidth, PRInt32 &aHeight);
   NS_IMETHOD GetClientRect(nsRect &aRect);
   NS_IMETHOD SupportsNativeWidgets( PRBool &aSupportsWidgets);
   NS_IMETHOD GetCanonicalPixelScale( float &aScale) const;
   NS_IMETHOD GetDeviceContextFor( nsIDeviceContextSpec *aDevice,
                                   nsIDeviceContext *&aContext);
   NS_IMETHOD CreateRenderingContext( nsIRenderingContext *&aContext);
   NS_IMETHOD BeginDocument();
   NS_IMETHOD EndDocument();
   NS_IMETHOD BeginPage();
   NS_IMETHOD EndPage();

   // OS2 specific methods
 public:
   // Call to ensure colour table/palette is loaded and ready.
   nsresult SetupColorMaps();
   // Get the palette object for the device, used to pick colours.
   // Release when done.
   nsresult GetPalette( nsIPaletteOS2 *&apalette);
   // Init from a HDC for printing purposes
   nsresult Init( nsNativeDeviceContext aContext,
                  nsIDeviceContext *aOrigContext);
   void CommonInit( HDC aDC);

   // Needed by the fontmetrics - can't rely on having a widget.
   HPS  GetRepresentativePS() const;
   void ReleaseRepresentativePS( HPS aPS);

 protected:
   virtual ~nsDeviceContextOS2();

   virtual nsresult CreateFontAliasTable();

   nsDrawingSurfaceOS2 *mSurface;
   PRUint32             mDepth;  // bit depth of device
   nsPaletteInfo        mPaletteInfo;
   nsIPaletteOS2       *mPalette;
   float                mPixelScale;
   PRInt32              mWidth;
   PRInt32              mHeight;
   HDC                  mDC;         // PrintDC.  Owned by libprint.
   HPS                  mPS;         // PrintPS.
   enum nsPrintState
   {
      nsPrintState_ePreBeginDoc,
      nsPrintState_eBegunDoc,
      nsPrintState_eBegunFirstPage,
      nsPrintState_eEndedDoc
   } mPrintState;
};

#endif
