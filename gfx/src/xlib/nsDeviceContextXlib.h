/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * David Smith <david@igelaus.com.au>
 */

#ifndef nsDeviceContextXlib_h__
#define nsDeviceContextXlib_h__

#include "nsDeviceContext.h"
#include "nsIRenderingContext.h"

#include <X11/Xlib.h>

class nsDeviceContextXlib : public DeviceContextImpl
{
public:
  nsDeviceContextXlib();

  NS_IMETHOD  Init(nsNativeWidget aNativeWidget);

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext)
    {return (DeviceContextImpl::CreateRenderingContext(aView, aContext)); }
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext)
    {return (DeviceContextImpl::CreateRenderingContext(aWidget, aContext)); }
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;
  NS_IMETHOD  GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const;

  NS_IMETHOD GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);

  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);
  NS_IMETHOD CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetRect(nsRect &aRect);
  NS_IMETHOD GetClientRect(nsRect &aRect);

  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext);

  NS_IMETHOD BeginDocument(void);
  NS_IMETHOD EndDocument(void);

  NS_IMETHOD BeginPage(void);
  NS_IMETHOD EndPage(void);

  Display * GetDisplay() { return mDisplay; }
  Screen * GetScreen() { return mScreen; }
  Visual * GetVisual() { return mVisual; }
  int      GetDepth() { return mDepth; }
	NS_IMETHOD GetDepth( PRUint32 &depth ) { depth=(PRUint32)mDepth;return NS_OK; }

protected:

  virtual ~nsDeviceContextXlib();

private:
  void                 CommonInit(void);
  nsPaletteInfo        mPaletteInfo;
  PRBool               mWriteable;
  PRUint32             mNumCells;
  nsDrawingSurface     mSurface;
  Display *            mDisplay;
  Screen *             mScreen;
  Visual *             mVisual;
  int                  mDepth;

  float                mWidthFloat;
  float                mHeightFloat;
  PRInt32              mWidth;
  PRInt32              mHeight;
};

#endif
