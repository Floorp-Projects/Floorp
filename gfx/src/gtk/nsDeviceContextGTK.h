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
 */

#ifndef nsDeviceContextGTK_h___
#define nsDeviceContextGTK_h___

#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"

#include "nsRenderingContextGTK.h"

class nsDeviceContextGTK : public DeviceContextImpl
{
public:
  nsDeviceContextGTK();
  virtual ~nsDeviceContextGTK();

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsNativeWidget aNativeWidget);

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aView,aContext));}
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aWidget,aContext));}

  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;
  NS_IMETHOD  GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.

  NS_IMETHOD GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);

  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);
  NS_IMETHOD CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetClientRect(nsRect &aRect);
  NS_IMETHOD GetRect(nsRect &aRect);

  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext);

  NS_IMETHOD BeginDocument(void);
  NS_IMETHOD EndDocument(void);

  NS_IMETHOD BeginPage(void);
  NS_IMETHOD EndPage(void);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  static int prefChanged(const char *aPref, void *aClosure);
  nsresult   SetDPI(PRInt32 dpi);
  
private:
  PRUint32      mDepth;
  PRBool        mWriteable;
  nsPaletteInfo mPaletteInfo;
  PRUint32      mNumCells;
  PRInt16       mScrollbarHeight;
  PRInt16       mScrollbarWidth;
  static nscoord mDpi;

  float         mWidthFloat;
  float         mHeightFloat;
  PRInt32       mWidth;
  PRInt32       mHeight;

  nsresult GetSystemFontInfo(GdkFont* iFont, nsSystemAttrID anID, nsFont* aFont) const;
};

#endif /* nsDeviceContextGTK_h___ */

