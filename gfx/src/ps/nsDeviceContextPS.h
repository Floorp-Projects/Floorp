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

#ifndef nsDeviceContextPS_h___
#define nsDeviceContextPS_h___

#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"
#include "nsVoidArray.h"
#include "nsIDeviceContextPS.h"

class nsPostScriptObj;
class nsDeviceContextWin;       // need to be a friend of the class using us.

class nsDeviceContextPS : public DeviceContextImpl,
                          public nsIDeviceContextPS
{
public:
  nsDeviceContextPS();

	NS_DECL_ISUPPORTS_INHERITED

  /**
   * This method does nothing since a postscript devicecontext will never be created
   * with a NativeWidget.
   * @update 12/21/98 dwc
   */
  NS_IMETHOD  InitDeviceContextPS(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext); 

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;

	void 				SetDrawingSurface(nsDrawingSurface  aSurface) { mSurface = aSurface; }
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);


  NS_IMETHOD 	  CheckFontExistence(const nsString& aFontName);
  NS_IMETHODIMP GetILColorSpace(IL_ColorSpace*& aColorSpace);
  NS_IMETHOD 	  GetDepth(PRUint32& aDepth);
  NS_IMETHOD 	  ConvertPixel(nscolor aColor, PRUint32 & aPixel);

  NS_IMETHOD 	GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD    GetClientRect(nsRect &aRect);

  NS_IMETHOD 	GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext);
  NS_IMETHOD  GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const;


  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics);
  NS_IMETHOD 	BeginDocument(void);
  NS_IMETHOD 	EndDocument(void);
  NS_IMETHOD 	BeginPage(void);
  NS_IMETHOD 	EndPage(void);

  
  NS_IMETHOD  SetSpec(nsIDeviceContextSpec *aSpec);

protected:
  virtual 	~nsDeviceContextPS();
  
  nsDrawingSurface 			mSurface;
  PRUint32 							mDepth;
  nsIDeviceContextSpec  *mSpec;
  float                 mPixelScale;
  nsVoidArray           mFontMetrics;  // we are not using the normal font cache, this is special for PostScript.
  nsPostScriptObj       *mPSObj;

public:
  nsPostScriptObj*    GetPrintContext() { return mPSObj; }

};

#endif /* nsDeviceContextPS_h___ */
