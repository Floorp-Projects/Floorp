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
#include "nsPrintManager.h"
#include "nsVoidArray.h"
#include "nsPSStructs.h"

extern "C" void xl_begin_document(PSContext *aCX);
extern "C" void xl_initialize_translation(PSContext *aCX, PrintSetup* aPS);
extern "C" void xl_end_document(PSContext *aCX);
extern "C" void xl_begin_page(PSContext *aCX, int aPN);
extern "C" void xl_end_page(PSContext *aCX, int aPN);
extern "C" void xl_finalize_translation(PSContext *aCX);
extern "C" void xl_show(PSContext *aCX, char* aTxt, int aLen, char *aAlign);
extern "C" void xl_translate(PSContext* aCX, int aX, int aY);
extern "C" void xl_moveto(PSContext* aCX, int aX, int aY);
extern "C" void xl_line(PSContext* aCX, int aX1, int aY1, int aX2, int aY2, int aThick);
extern "C" void xl_box(PSContext* aCX, int w, int h);
extern "C" void xl_moveto_loc(PSContext* aCX, int aX, int aY);
extern "C" void xl_lineto(PSContext* aCX, int aX1, int aY1);
extern "C" void xl_stroke(PSContext* aCX);
extern "C" void xl_fill(PSContext* aCX);
extern "C" void xl_closepath(PSContext* aCX);
extern "C" void xl_graphics_save(PSContext *aCX);
extern "C" void xl_graphics_restore(PSContext *aCX);

class nsDeviceContextWin;       // need to be a friend of the class using us.

class nsDeviceContextPS : public DeviceContextImpl
{
public:
  nsDeviceContextPS();

	NS_DECL_ISUPPORTS

  /**
   * This method does nothing since a postscript devicecontext will never be created
   * with a NativeWidget.
   * @update 12/21/98 dwc
   */
  NS_IMETHOD  Init(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext, HDC aTheDC);  

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;

	void 				SetDrawingSurface(nsDrawingSurface  aSurface) { mSurface = aSurface; }
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);


  NS_IMETHOD 	  CheckFontExistence(const nsString& aFontName);
  //NS_IMETHOD 	  CreateILColorSpace(IL_ColorSpace*& aColorSpace);
  NS_IMETHODIMP GetILColorSpace(IL_ColorSpace*& aColorSpace);
  NS_IMETHOD 	  GetDepth(PRUint32& aDepth);
  NS_IMETHOD 	  ConvertPixel(nscolor aColor, PRUint32 & aPixel);

  NS_IMETHOD 	GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);

  NS_IMETHOD 	GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext);
  NS_IMETHOD  GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const;


  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics);
  NS_IMETHOD 	BeginDocument(void);
  NS_IMETHOD 	EndDocument(void);

  NS_IMETHOD 	BeginPage(void);
  NS_IMETHOD 	EndPage(void);

protected:
  virtual 	~nsDeviceContextPS();
  
  nsDrawingSurface 			mSurface;
  PRUint32 							mDepth;
  PSContext             *mPrintContext; 
  nsIDeviceContextSpec  *mSpec;
  PrintSetup            *mPrintSetup;
  float                 mPixelScale;
  nsVoidArray           mFontMetrics;  // we are not using the normal font cache, this is special for PostScript.
  PRUint16              mPageNumber;


public:
  //static bool   GetMacFontNumber(const nsString& aFontName, short &fontNum);
  PSContext*    GetPrintContext() { return mPrintContext; }

public:
  HDC           mDC;               // this is temporary!!!

friend nsDeviceContextWin;         // need to be a friend of the class using us.
};

#endif /* nsDeviceContextPS_h___ */
