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

#ifndef nsRenderingContextPS_h___
#define nsRenderingContextPS_h___

#include "nsIRenderingContext.h"
#include "nsUnitConversion.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsPoint.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsTransform2D.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsRect.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsPrintManager.h"
#include "nsPSStructs.h"

class PS_State;
typedef void* nsDrawingSurfacePS;

class nsRenderingContextPS : public nsIRenderingContext
{
public:
  nsRenderingContextPS();
  ~nsRenderingContextPS();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  // nsIPrinterRenderingContext methods

protected:

public:
  // nsIRenderingContext
  NS_IMETHOD Init(nsIDeviceContext* aContext);
  NS_IMETHOD Init(nsIDeviceContext* aContext, nsIWidget *aWidget){return NS_OK;}
  NS_IMETHOD Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface){return NS_OK;}

  NS_IMETHOD Reset(void);

  NS_IMETHOD GetDeviceContext(nsIDeviceContext *&aContext);

  NS_IMETHOD SelectOffScreenDrawingSurface(nsDrawingSurface aSurface);
  NS_IMETHOD GetHints(PRUint32& aResult);

  NS_IMETHOD PushState(void);
  NS_IMETHOD PopState(PRBool &aClipState);

  NS_IMETHOD IsVisibleRect(const nsRect& aRect, PRBool &aClipState);

  NS_IMETHOD SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aCilpState);
  NS_IMETHOD GetClipRect(nsRect &aRect, PRBool &aClipState);
  NS_IMETHOD SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipState);
  NS_IMETHOD GetClipRegion(nsIRegion **aRegion);

  NS_IMETHOD SetLineStyle(nsLineStyle aLineStyle);
  NS_IMETHOD GetLineStyle(nsLineStyle &aLineStyle);

  NS_IMETHOD SetColor(nscolor aColor);
  NS_IMETHOD GetColor(nscolor &aColor) const;

  NS_IMETHOD SetFont(const nsFont& aFont);
  NS_IMETHOD SetFont(nsIFontMetrics *aFontMetrics);

  NS_IMETHOD GetFontMetrics(nsIFontMetrics *&aFontMetrics);

  NS_IMETHOD Translate(nscoord aX, nscoord aY);
  NS_IMETHOD Scale(float aSx, float aSy);
  NS_IMETHOD GetCurrentTransform(nsTransform2D *&aTransform);

  NS_IMETHOD CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface);
  NS_IMETHOD DestroyDrawingSurface(nsDrawingSurface aDS);

  NS_IMETHOD DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
  NS_IMETHOD DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints);

  NS_IMETHOD DrawRect(const nsRect& aRect);
  NS_IMETHOD DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD FillRect(const nsRect& aRect);
  NS_IMETHOD FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  NS_IMETHOD DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
  NS_IMETHOD FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);

  NS_IMETHOD DrawEllipse(const nsRect& aRect);
  NS_IMETHOD DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD FillEllipse(const nsRect& aRect);
  NS_IMETHOD FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  NS_IMETHOD DrawArc(const nsRect& aRect,
                     float aStartAngle, float aEndAngle);
  NS_IMETHOD DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                     float aStartAngle, float aEndAngle);
  NS_IMETHOD FillArc(const nsRect& aRect,
                     float aStartAngle, float aEndAngle);
  NS_IMETHOD FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                     float aStartAngle, float aEndAngle);

  NS_IMETHOD GetWidth(char aC, nscoord& aWidth);
  NS_IMETHOD GetWidth(PRUnichar aC, nscoord& aWidth,
                      PRInt32 *aFontID);
  NS_IMETHOD GetWidth(const nsString& aString, nscoord& aWidth,
                      PRInt32 *aFontID);
  NS_IMETHOD GetWidth(const char* aString, nscoord& aWidth);
  NS_IMETHOD GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth);
  NS_IMETHOD GetWidth(const PRUnichar* aString, PRUint32 aLength,
                      nscoord& aWidth, PRInt32 *aFontID);

  NS_IMETHOD DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        const nscoord* aSpacing);
  NS_IMETHOD DrawString(const PRUnichar *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        PRInt32 aFontID,
                        const nscoord* aSpacing);
  NS_IMETHOD DrawString(const nsString& aString, nscoord aX, nscoord aY,
                        PRInt32 aFontID,
                        const nscoord* aSpacing);

  NS_IMETHOD DrawImage(nsIImage *aImage, nscoord aX, nscoord aY);
  NS_IMETHOD DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                       nscoord aWidth, nscoord aHeight); 
  NS_IMETHOD DrawImage(nsIImage *aImage, const nsRect& aRect);
  NS_IMETHOD DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect);

  NS_IMETHOD CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                               const nsRect &aDestBounds, PRUint32 aCopyFlags);


  // Postscript utilities
  /** ---------------------------------------------------
   *  Set the color of the Postscript to a file set up by the nsDeviceContextPS
   *	@update 12/21/98 dwc
   */
  void PostscriptColor(nscolor aColor);

  /** ---------------------------------------------------
   *  Fill a postscript rectangle to a file set up by the nsDeviceContextPS
   *	@update 12/21/98 dwc
   */
  void PostscriptFillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  /** ---------------------------------------------------
   *  Render a bitmap in postscript to a file set up by the nsDeviceContextPS
   *	@update 12/21/98 dwc
   */
  void PostscriptDrawBitmap(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, IL_Pixmap *aImage, IL_Pixmap *aMask);

  /** ---------------------------------------------------
   *  Set the current postscript font
   *	@update 12/21/98 dwc
   */
  void PostscriptFont(nscoord aHeight, PRUint8 aStyle, 
			  PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations);

  /** ---------------------------------------------------
   *  Output postscript text to a file set up by the nsDeviceContextPS
   *	@update 12/21/98 dwc
   */  
  void PostscriptTextOut(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing, PRBool aIsUnicode);


private:
  nsresult CommonInit(void);
  void SetupFontAndColor(void);
  void PushClipState(void);

protected:
  nsIDeviceContext    *mContext;
  nsIFontMetrics	    *mFontMetrics;
  nsLineStyle          mCurrLineStyle;
  PS_State            *mStates;
  nsVoidArray         *mStateCache;
  nsTransform2D		    *mTMatrix;
  float                mP2T;


  //state management
  PRUint8             *mGammaTable;
  PSContext           *mPrintContext; //XXX: Remove need for MWContext


};


#endif /* nsRenderingContextPS_h___ */
