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

#ifndef nsDeviceContextXP_h___
#define nsDeviceContextXP_h___

#include <X11/Xlib.h>
#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"
#include "nsVoidArray.h"
#include "nsIDeviceContextXPrint.h"

class nsXPrintContext;

class nsDeviceContextXP : public DeviceContextImpl,
                          public nsIDeviceContextXP
{
public:
  nsDeviceContextXP();

	NS_DECL_ISUPPORTS_INHERITED

  /**
   * This method does nothing since a postscript devicecontext will never be created
   * with a NativeWidget.
   * @update 12/21/98 dwc
   */
  NS_IMETHOD  InitDeviceContextXP(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext); 

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aView,aContext));}
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aWidget,aContext));}

  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;

  void                            SetDrawingSurface(nsDrawingSurface  aSurface) { mSurface = aSurface; }
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);

  NS_IMETHOD 	  CheckFontExistence(const nsString& aFontName);
  NS_IMETHODIMP GetILColorSpace(IL_ColorSpace*& aColorSpace);
  NS_IMETHOD 	  GetDepth(PRUint32& aDepth);
  NS_IMETHOD 	  ConvertPixel(nscolor aColor, PRUint32 & aPixel);

  NS_IMETHOD 	GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetRect(nsRect &aRect);
  NS_IMETHOD    GetClientRect(nsRect &aRect);

  NS_IMETHOD 	GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext);
  NS_IMETHOD  GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const;
  NS_IMETHOD 	BeginDocument(PRUnichar * aTitle);
  NS_IMETHOD 	EndDocument(void);
  NS_IMETHOD 	BeginPage(void);
  NS_IMETHOD 	EndPage(void);

  NS_IMETHOD  SetSpec(nsIDeviceContextSpec *aSpec);

  Display * GetDisplay(); 
  nsXPrintContext *GetPrintContext() { return mPrintContext; }

  NS_IMETHOD    GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics);
  NS_IMETHOD    GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup, nsIFontMetrics*& aMetrics);

protected:
  virtual 	~nsDeviceContextXP();

  nsDrawingSurface       mSurface ;
  nsXPrintContext 	*mPrintContext;  
  Display		*mDisplay;
  Screen		*mScreen;
  PRUint32 		mDepth;
  nsIDeviceContextSpec  *mSpec;
  float                 mPixelScale;
  nsVoidArray           mFontMetrics;  // we are not using the normal font cache, this is special for PostScript.

private:
  void                 CommonInit(void);
  nsPaletteInfo        mPaletteInfo;
  PRBool               mWriteable;
  PRUint32             mNumCells;

  float                mWidthFloat;
  float                mHeightFloat;
  PRInt32              mWidth;
  PRInt32              mHeight;

};

#endif /* nsDeviceContextPS_h___ */
