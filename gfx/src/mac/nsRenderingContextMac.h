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

#ifndef nsRenderingContextMac_h___
#define nsRenderingContextMac_h___

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
#include "nsImageMac.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsIRegion.h"
#include "nsDeviceContextMac.h"

class GraphicsState;

typedef GrafPtr nsDrawingSurfaceMac;

class nsRenderingContextMac : public nsIRenderingContext
{
public:
  nsRenderingContextMac();
  ~nsRenderingContextMac();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsIDeviceContext* aContext, nsIWidget *aWindow);
  virtual nsresult Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface);
  virtual nsresult CommonInit();

  virtual void Reset();
  virtual nsIDeviceContext * GetDeviceContext(void);
  virtual nsresult	SelectOffScreenDrawingSurface(nsDrawingSurface aSurface);
  NS_IMETHOD GetHints(PRUint32& aResult){return NS_OK;};
  virtual void		PushState(void);
  virtual PRBool 	PopState(void);
  virtual PRBool	IsVisibleRect(const nsRect& aRect);
  virtual PRBool	SetClipRectInPixels(const nsRect& aRect, nsClipCombine aCombine);
  virtual PRBool	SetClipRect(const nsRect& aRect, nsClipCombine aCombine);
  virtual PRBool	GetClipRect(nsRect &aRect);
  virtual PRBool	SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine);
  virtual void 		GetClipRegion(nsIRegion **aRegion);
  NS_IMETHOD      SetLineStyle(nsLineStyle aLineStyle);
  NS_IMETHOD      GetLineStyle(nsLineStyle &aLineStyle);
  virtual void 		SetColor(nscolor aColor);
  virtual nscolor GetColor() const;
  virtual void 		SetFont(const nsFont& aFont);
  virtual void    SetFont(nsIFontMetrics *aFontMetrics);
  virtual const nsFont& GetFont();
  virtual nsIFontMetrics * GetFontMetrics();
  virtual void 		Translate(nscoord aX, nscoord aY);
  virtual void 		Scale(float aSx, float aSy);
  virtual nsTransform2D * GetCurrentTransform();
  virtual nsDrawingSurface CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags);
  virtual void 		DestroyDrawingSurface(nsDrawingSurface aDS);
  virtual void 		DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
  virtual void 		DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints);
  virtual void 		DrawRect(const nsRect& aRect);
  virtual void 		DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  virtual void 		FillRect(const nsRect& aRect);
  virtual void 		FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  virtual void 		DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
  virtual void 		FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
  virtual void 		DrawEllipse(const nsRect& aRect);
  virtual void 		DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  virtual void 		FillEllipse(const nsRect& aRect);
  virtual void 		FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  virtual void 		DrawArc(const nsRect& aRect,float aStartAngle, float aEndAngle);
  virtual void 		DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,float aStartAngle, float aEndAngle);
  virtual void 		FillArc(const nsRect& aRect,float aStartAngle, float aEndAngle);
  virtual void 		FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,float aStartAngle, float aEndAngle);
  NS_IMETHOD  GetWidth(char aC, nscoord &aWidth);
  NS_IMETHOD  GetWidth(PRUnichar aC, nscoord &aWidth);
  NS_IMETHOD  GetWidth(const nsString& aString, nscoord &aWidth);
  NS_IMETHOD  GetWidth(const char *aString, nscoord &aWidth);
  NS_IMETHOD  GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth);
  NS_IMETHOD  GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth);
  virtual void 		DrawString(const char *aString, PRUint32 aLength,nscoord aX, nscoord aY,nscoord aWidth, const nscoord* aSpacing);
  virtual void 		DrawString(const PRUnichar *aString, PRUint32 aLength, nscoord aX, nscoord aY,nscoord aWidth, const nscoord* aSpacing);
  virtual void 		DrawString(const nsString& aString, nscoord aX, nscoord aY,nscoord aWidth, const nscoord* aSpacing);
  virtual void 		DrawImage(nsIImage *aImage, nscoord aX, nscoord aY);
  virtual void 		DrawImage(nsIImage *aImage, nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight); 
  virtual void 		DrawImage(nsIImage *aImage, const nsRect& aRect);
  virtual void 		DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect);
  NS_IMETHOD  CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                const nsRect &aDestBounds, PRUint32 aCopyFlags);
	
protected:
  nscolor 							mCurrentColor ;
  nsTransform2D		  		*mTMatrix;					// transform that all the graphics drawn here will obey
  float             		mP2T;
  nsDrawingSurfaceMac		mRenderingSurface;  // main drawing surface,Can be a BackBuffer if Selected in
  nsDrawingSurfaceMac		mFrontBuffer;				// current buffer to draw into
	nsDrawingSurfaceMac		mOriSurface;
  nsIDeviceContext			*mContext;
  nsIFontMetrics				*mFontMetrics;
  RgnHandle							mClipRegion;
  RgnHandle							mMainRegion;
  PRInt32               mCurrFontHandle;
  PRInt32								mOffx;
  PRInt32								mOffy;

  //state management
  nsVoidArray       *mStateCache;

};

#endif /* nsRenderingContextMac_h___ */
