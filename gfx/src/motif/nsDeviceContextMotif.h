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

#ifndef nsDeviceContextMotif_h___
#define nsDeviceContextMotif_h___

#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"

#include "X11/Xlib.h"
#include "X11/Intrinsic.h"

#ifdef MITSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

/* nsDrawingSurface is actually the following struct */
struct nsDrawingSurfaceMotif {
  Display *display ;
  Drawable drawable ;
  GC       gc ;
  Visual * visual ;
  PRUint32 depth ;
#ifdef MITSHM
  XShmSegmentInfo shmInfo;
  XImage * shmImage;
#endif
};


class nsDeviceContextMotif : public DeviceContextImpl 
{
public:
  nsDeviceContextMotif();

  NS_DECL_ISUPPORTS

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.

  NS_IMETHOD  GetILColorSpace(IL_ColorSpace*& aColorSpace);
  NS_IMETHOD  GetPaletteInfo(nsPaletteInfo&);
  NS_IMETHOD  Init(nsNativeWidget aNativeWidget);
  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;
  NS_IMETHOD  GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const;
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);

  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);


  NS_IMETHOD CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetClientRect(nsRect &aRect);

  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext);

  NS_IMETHOD BeginDocument(void);
  NS_IMETHOD EndDocument(void);

  NS_IMETHOD BeginPage(void);
  NS_IMETHOD EndPage(void);


protected:
  ~nsDeviceContextMotif();
  void AllocColors();
  uint8 AllocColor(uint8 aRed, uint8 aGreen, 
                   uint8 aBlue, PRBool aCanAlloc);



  nsDrawingSurfaceMotif * mSurface ;

  PRUint32      mDepth;
  Visual      * mVisual;
  PRBool        mWriteable;
  PRUint32      mNumCells;
  Colormap      mColormap;
  nsPaletteInfo mPaletteInfo;
  PRBool        mColorsAllocated;
  uint8*        mIndex;
  XColor*       mDeviceColors;
  Display*      mDisplay;

public:
  void InstallColormap(void);
  void InstallColormap(Display* aDisplay, Drawable aDrawable);
  void SetDrawingSurface(nsDrawingSurfaceMotif * aSurface) { mSurface = aSurface; }
  nsDrawingSurface GetDrawingSurface();
  Display *GetDisplay();

private:
  PRUint32 mRedMask;
  PRUint32 mGreenMask;
  PRUint32 mBlueMask;
  PRUint32 mRedBits;
  PRUint32 mGreenBits;
  PRUint32 mBlueBits;
  PRUint32 mRedOffset;
  PRUint32 mGreenOffset;
  PRUint32 mBlueOffset;

};

#endif /* nsDeviceContextMotif_h___ */
